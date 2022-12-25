// Include GLEW
#include <GL/glew.h>

// Include GLFW
#include <GLFW/glfw3.h>

#include <chrono>
#include <thread>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define LM_DEBUG_INTERPOLATION
#define LIGHTMAPPER_IMPLEMENTATION
#include <lightmapper.h>
#include <indicators.hpp>

#include "lightmapper.hpp"
#include <camera/globals.hpp>
#include "editor.hpp"
#include "texture.hpp"

#include "assets.hpp"
#include "level.hpp"

static void writeFrambufferToTga(std::string_view path, int width, int height) {
	int* buffer = new int[width * height * 3];
	glReadPixels(0, 0, width, height, GL_BGR, GL_UNSIGNED_BYTE, buffer);
	FILE* fp = fopen(path.data(), "wb");
	if (fp == NULL)
	{
		fprintf(stderr, "Failed to open file %s to write TGA.",
			path.data());
		return;
	}

	printf("----------------Writing Frame to TGA %s----------------\n", path.data());
	short TGAhead[] = { 0, 2, 0, 0, 0, 0, (short)width, (short)height, 24 };
	fwrite(TGAhead, sizeof(TGAhead), 1, fp);
	fwrite(buffer, 3 * width * height, 1, fp);
	delete[] buffer;

	fclose(fp);
}

glm::mat4x4 getShadowMatrixFromRadius(Environment& env, glm::vec3 c, float r) {
	auto min_c = c - glm::vec3(r);
	auto max_c = c - glm::vec3(r);

	const auto shadow_view = glm::lookAt(-env.sun_direction + c, c, glm::vec3(0,1,0));
	const auto shadow_projection = glm::ortho(min_c.x, max_c.x, min_c.y, max_c.y, min_c.z, max_c.z);
	return shadow_projection * shadow_view;
}

void createShadowMapForGeometry(RenderQueue& q, Environment& env, Camera &camera, glm::vec3 position) {
	const double& cnp = camera.frustrum.near_plane, & cfp = camera.frustrum.far_plane / 10.0f; // Only consider effects of close shadows
	double r;
	for (int i = 0; i < SHADOW_CASCADE_NUM; i++) {
		const double p = (double)(i + 1) / (double)SHADOW_CASCADE_NUM;

		// Simple linear formula
		r = cnp + (cfp - cnp) * p;

		graphics::shadow_cascade_distances[i] = r;
		graphics::shadow_vps[i] = getShadowMatrixFromRadius(env, position, r);
	}

	writeShadowVpsUbo();

	int old_shadow_size = graphics::shadow_size;
	graphics::shadow_size = 1024;
	drawRenderQueueShadows(q);
	graphics::shadow_size = old_shadow_size;
}

bool runLightmapper(Level& level, AssetManager &asset_manager) {
	lm_context* ctx = lmCreate(
		16,               // hemicube rendering resolution/quality
		0.1f, 10.0f,      // zNear, zFar
		1.0f, 1.0f, 1.0f, // sky/clear color
		2, 0.1f,          // hierarchical selective interpolation for speedup (passes, threshold)
		0.1f);            // modifier for camera-to-surface distance for hemisphere rendering.
						  // tweak this to trade-off between interpolated vertex normal quality and other artifacts (see declaration).
	if (!ctx) {
		std::cerr << "Could not initialize lightmapper.\n";
		return false;
	}

	// @todo dynamic lightmap resolution / editor
	constexpr int max_w = 1024;
	constexpr int max_h = 1024;
	constexpr int min_w = 128;
	constexpr int min_h = 128;
	constexpr float min_aabb_size = 0.5;
	constexpr float max_aabb_size = 10.0;
	constexpr int c = 3;

	// Buffer used during post processing
	float* temp = (float*)malloc(max_w * max_h * c * sizeof(float));

	// @todo make global state of renderer contained in a struct
	auto old_window_width = window_width;
	auto old_window_height = window_height;
	auto old_bloom = graphics::do_bloom;
	auto old_shadows = graphics::do_shadows;
	auto old_volumetrics = graphics::do_volumetrics;
	auto old_msaa = graphics::do_msaa;

	graphics::do_bloom = false;
	graphics::do_shadows = false;
	graphics::do_volumetrics = false;
	graphics::do_msaa = false;
	window_width = max_w;
	window_height = max_h;
	initHdrFbo(true);

	// Camera which is moved by the lightmapper
	Frustrum frustrum;
	frustrum.near_plane = 0.1f;
	frustrum.far_plane = 10.0f;
	Camera camera(frustrum);

	std::cout << "====================================================================================\n"
				 "\tBaking " << level.path << "'s Lightmap\n"
				 "====================================================================================\n";

	std::vector<float*> lightmaps;
	std::vector<glm::ivec2> dimensions;
	std::vector<glm::mat4> models;
	std::vector<MeshEntity*> mesh_entities;
	std::vector<uint64_t> submesh_indices;
	for (int i = 0; i < ENTITY_COUNT; ++i) {
		auto m_e = reinterpret_cast<MeshEntity*>(level.entities.entities[i]);
		if (m_e == nullptr ||
			!entityInherits(m_e->type, MESH_ENTITY) || m_e->mesh == nullptr || !m_e->do_lightmap || m_e->mesh->uvs == nullptr ||
			entityInherits(m_e->type, ANIMATED_MESH_ENTITY))
			continue;

		for (int j = m_e->mesh->num_submeshes-1; j >= m_e->mesh->num_submeshes - 1; j--) {
			auto& model = models.emplace_back(createModelMatrix(m_e->mesh->transforms[j], m_e->position, m_e->rotation, m_e->scale));
			auto t_aabb = transformAABB(m_e->mesh->aabbs[j], model);
			float dimension_t = linearstep(min_aabb_size, max_aabb_size, glm::length(t_aabb.size));
			auto& dimension = dimensions.emplace_back(glm::mix(glm::vec2(min_w, min_h), glm::vec2(max_w, max_h), dimension_t));

			float* img = (float*)malloc(dimension.x * dimension.y * c * sizeof(float));
			assert(img);

			lightmaps.push_back(img);
			mesh_entities.push_back(m_e);
			submesh_indices.push_back(j);

			// Make a new material which is unique to the submesh
			Material* new_mat;
			const auto& overide_mat_lu = m_e->overidden_materials.find(j);
			if (overide_mat_lu != m_e->overidden_materials.end()) {
				new_mat = &overide_mat_lu->second;
			} else {
				new_mat = &m_e->overidden_materials.emplace(j, m_e->mesh->materials[m_e->mesh->material_indices[j]]).first->second;
			}
			// Add a black lightmap texture to this material
			new_mat->textures[TextureSlot::GI] = asset_manager.getColorTexture(glm::vec4(0.0), GL_RGB);
			new_mat->type |= MaterialType::LIGHTMAPPED;
			if (new_mat->uniforms.find("ambient_mult") == new_mat->uniforms.end()) {
				new_mat->uniforms.emplace("ambient_mult", 1.0f);
			}
		}
	}

	RenderQueue q;
	createRenderQueue(q, level.entities, true);

	constexpr int bounces = 1;
	for (int b = 0; b < bounces; b++) {
		std::cout << "Bounce " << b << " / " << bounces - 1 << "\n";

		int count = 1;
		// render all static geometry to their lightmaps
		for(int i = 0; i < lightmaps.size(); ++i) {
			const auto& img = lightmaps[i];
			const auto& m_e = mesh_entities[i];
			const auto& submesh_i = submesh_indices[i];
			const auto& dimension = dimensions[i];
			const auto& model = models[i];
			auto& mesh = m_e->mesh;

			// @note relies on 0 bits representing 0.0 in floating format which is true for almost all standards
			memset(img, 0, sizeof(float) * dimension.x * dimension.y * c); // clear lightmap to black
			lmSetTargetLightmap(ctx, img, dimension.x, dimension.y, c);

			indicators::ProgressBar bar{ indicators::option::BarWidth{50}, indicators::option::Start{"["}, indicators::option::Fill{"="}, indicators::option::Lead{">"}, indicators::option::Remainder{" "}, indicators::option::End{" ]"}, indicators::option::PostfixText{""}, indicators::option::ForegroundColor{indicators::Color::white}, indicators::option::FontStyles{std::vector<indicators::FontStyle>{indicators::FontStyle::bold}} };
			bar.set_option(indicators::option::ShowPercentage{ true });
			
			//createShadowMapForGeometry(q, camera, m_e->position);
			
			lmSetGeometry(ctx, (float*)&model[0][0],
				LM_FLOAT, mesh->vertices,	sizeof(*mesh->vertices),
				LM_FLOAT, mesh->normals,	sizeof(*mesh->normals),
				LM_FLOAT, mesh->uvs,		sizeof(*mesh->uvs),
				mesh->draw_count[submesh_i], LM_UNSIGNED_INT, &mesh->indices[mesh->draw_start[submesh_i]]);

			bar.set_option(indicators::option::PostfixText{ "Baking lightmap " + std::to_string(count) + " / " + std::to_string(lightmaps.size())});
			double last_update_time = 0;

			int viewport[4];
			while (lmBegin(ctx, viewport, &camera.view[0][0], &camera.projection[0][0])) {
				// Place camera at sample, looking in direction of normal,
				// set vp from the view and projection
				camera.position = glm::vec3(
					ctx->meshPosition.sample.position.x,
					ctx->meshPosition.sample.position.y,
					ctx->meshPosition.sample.position.z
				);
				camera.forward = glm::vec3(
					ctx->meshPosition.sample.direction.x,
					ctx->meshPosition.sample.direction.y,
					ctx->meshPosition.sample.direction.z
				);
				camera.up = glm::vec3(
					ctx->meshPosition.sample.up.x,
					ctx->meshPosition.sample.up.y,
					ctx->meshPosition.sample.up.z
				);
				camera.right = glm::cross(camera.forward, camera.up);
				camera.vp = camera.projection * camera.view;

				gl_state.bind_viewport(viewport[0], viewport[1], viewport[2], viewport[3]);

				frustrumCullRenderQueue(q, camera);
				drawRenderQueue(q, level.environment, camera);
				uncullRenderQueue(q);

				// Used for debugging the hemicube's rendering
				//using namespace std::this_thread; // sleep_for, sleep_until
				//using namespace std::chrono; // nanoseconds, system_clock, seconds
				//bindBackbuffer();
				//clearFramebuffer();
				//gl_state.bind_viewport(viewport[0], viewport[1], viewport[2], viewport[3]);
				//drawRenderQueue(q, skybox, skybox_irradiance, skybox_specular, camera);
				//sleep_for(milliseconds(10));
				//glfwSwapBuffers(window);
					
				double time = glfwGetTime();
				if (time - last_update_time > 2.0) {
					last_update_time = time;
					bar.set_progress(lmProgress(ctx) * 100.0f);
				}

				lmEnd(ctx);
			}
			bar.set_progress(100);
			count++;
		}

		indicators::ProgressBar bar{ indicators::option::BarWidth{50}, indicators::option::Start{"["}, indicators::option::Fill{"="}, indicators::option::Lead{">"}, indicators::option::Remainder{" "}, indicators::option::End{" ]"}, indicators::option::PostfixText{""}, indicators::option::ForegroundColor{indicators::Color::white}, indicators::option::FontStyles{std::vector<indicators::FontStyle>{indicators::FontStyle::bold}} };
		bar.set_option(indicators::option::ShowPercentage{ true });
		if (b == bounces - 1) {
			indicators::ProgressBar bar{ indicators::option::BarWidth{50}, indicators::option::Start{"["}, indicators::option::Fill{"="}, indicators::option::Lead{">"}, indicators::option::Remainder{" "}, indicators::option::End{" ]"}, indicators::option::PostfixText{""}, indicators::option::ForegroundColor{indicators::Color::white}, indicators::option::FontStyles{std::vector<indicators::FontStyle>{indicators::FontStyle::bold}} };
			bar.set_option(indicators::option::ShowPercentage{ true });
		}

		count = 1;
		// postprocess and upload all lightmaps to the GPU now to use them for indirect lighting during the next bounce.
		for (int i = 0; i < lightmaps.size(); ++i) {
			const auto& img = lightmaps[i];
			const auto& m_e = mesh_entities[i];
			const auto& dimension = dimensions[i];
			const auto& submesh_i = submesh_indices[i];
			const auto& mesh = *m_e->mesh;

			bar.set_progress((float)(count) / (float)lightmaps.size());
			bar.set_option(indicators::option::PostfixText{ "Postprocessing " + std::to_string(count) + " / " + std::to_string(lightmaps.size()) });

			lmImageSmooth(img, temp, dimension.x, dimension.y, c);
			lmImageDilate(temp, img, dimension.x, dimension.y, c);
			// @hardcoded gamma
			lmImagePower(img, dimension.x, dimension.y, c, 1.0f / 2.2f, 0x7); // gamma correct color channels

			// Generate a new texture seperate from the color texture (which is still stored by asset manager)
			Texture* lightmap;
			if (b == 0) {
				lightmap = asset_manager.createTexture(level.path + "." + std::to_string(i) + ".tga");
				glGenTextures(1, &lightmap->id);
			} else {
				lightmap = asset_manager.getTexture(level.path + "." + std::to_string(i) + ".tga");
				assert(lightmap);
			}
			lightmap->format = GL_RGB16F;
			lightmap->resolution = glm::ivec2(dimension.x, dimension.y);

			gl_state.bind_texture_any(lightmap->id);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, dimension.x, dimension.y, 0, GL_RGB, GL_FLOAT, img);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glGenerateMipmap(GL_TEXTURE_2D);
			lightmap->complete = true;

			// Update the material to show the lightmap
			m_e->overidden_materials[submesh_i].textures[TextureSlot::GI] = lightmap;

			// If we are on the last bound we want to save to disk
			if (b == bounces - 1) {
				const auto& img = lightmaps[i];
				const auto& m_e = mesh_entities[i];

				bar.set_option(indicators::option::PostfixText{ "Writing TGA " + std::to_string(count) + " / " + std::to_string(lightmaps.size()) });
				lmImageSaveTGAf(lightmap->handle.c_str(), img, dimension.x, dimension.y, c, 1.0);
			}

			count++;
		}
		bar.set_progress(100);
	}
	
	// Free memory, @todo chunk the lightmapping so memory usage is managed
	lmDestroy(ctx);
	for (auto& img : lightmaps) {
		free(img);
	}
	free(temp);

	// Restore global graphics state
	window_width = old_window_width;
	window_height = old_window_height;
	gl_state.bind_viewport(window_width, window_height);
	graphics::do_bloom = old_bloom;
	graphics::do_msaa = old_msaa;
	graphics::do_shadows = old_shadows;
	graphics::do_volumetrics = old_volumetrics;
	initHdrFbo(true);

	// Update shadow vp for correct camera
	updateShadowVP(*Cameras::get_active_camera(), loaded_level.environment.sun_direction);
	return true;
}
