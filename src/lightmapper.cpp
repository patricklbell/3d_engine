// Include GLEW
#include <GL/glew.h>

// Include GLFW
#include <GLFW/glfw3.h>

#include <chrono>
#include <thread>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

//#define LM_DEBUG_INTERPOLATION
#define LIGHTMAPPER_IMPLEMENTATION
#include <lightmapper.h>
#include <indicators.hpp>

#include "lightmapper.hpp"
#include <camera/globals.hpp>
#include "editor.hpp"
#include "texture.hpp"
#include "utilities.hpp"
#include "assets.hpp"

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

glm::mat4x4 getShadowMatrixFromRadius(glm::vec3 c, float r) {
	auto min_c = c - glm::vec3(r);
	auto max_c = c - glm::vec3(r);

	const auto shadow_view = glm::lookAt(-sun_direction + c, c, glm::vec3(0,1,0));
	const auto shadow_projection = glm::ortho(min_c.x, max_c.x, min_c.y, max_c.y, min_c.z, max_c.z);
	return shadow_projection * shadow_view;
}

void createShadowMapForGeometry(RenderQueue& q, Camera &camera, glm::vec3 position) {
	const double& cnp = camera.frustrum.near_plane, & cfp = camera.frustrum.far_plane / 10.0f; // Only consider effects of close shadows
	double r;
	for (int i = 0; i < SHADOW_CASCADE_NUM; i++) {
		const double p = (double)(i + 1) / (double)SHADOW_CASCADE_NUM;

		// Simple linear formula
		r = cnp + (cfp - cnp) * p;

		graphics::shadow_cascade_distances[i] = r;
		graphics::shadow_vps[i] = getShadowMatrixFromRadius(position, r);
	}

	writeShadowVpsUbo();

	int old_shadow_size = graphics::shadow_size;
	graphics::shadow_size = 1024;
	drawRenderQueueShadows(q);
	graphics::shadow_size = old_shadow_size;
}

bool runLightmapper(EntityManager& entity_manager, AssetManager &asset_manager, Texture* skybox, Texture *skybox_irradiance, Texture *skybox_specular) {
	lm_context* ctx = lmCreate(
		64,               // hemicube rendering resolution/quality
		0.01f, 100.0f,    // zNear, zFar
		1.0f, 1.0f, 1.0f, // sky/clear color
		4, 0.1f,         // hierarchical selective interpolation for speedup (passes, threshold)
		0.01f);           // modifier for camera-to-surface distance for hemisphere rendering.
						  // tweak this to trade-off between interpolated vertex normal quality and other artifacts (see declaration).
	if (!ctx) {
		std::cerr << "Could not initialize lightmapper.\n";
		return false;
	}

	// @todo dynamic lightmap resolution / editor
	constexpr int w = 654; //image height
	constexpr int h = 654; //image height
	constexpr int c = 4;

	// Buffer used during post processing
	float* temp = (float*)malloc(w * h * c * sizeof(float));

	auto old_window_width = window_width;
	auto old_window_height = window_height;
	auto old_bloom = graphics::do_bloom;
	auto old_shadows = graphics::do_shadows;
	auto old_msaa = graphics::do_msaa;
	auto old_sun_color = sun_color; // Hacky way to get rid of direct contribution

	sun_color = glm::vec3(0.0);
	graphics::do_bloom = false;
	graphics::do_msaa = false;
	graphics::do_shadows = true;
	window_width = w;
	window_height = h;
	initHdrFbo();

	// Not a real camera, just so shader uniforms are correct
	Camera::Frustrum frustrum;
	frustrum.near_plane = 0.01f;
	frustrum.far_plane = 100.0f;
	Camera camera(frustrum);

	std::cout << "====================================================================================\n"
				 "\tBaking " << level_path << "'s Lightmap\n"
				 "====================================================================================\n";

	std::vector<float*> lightmaps;
	std::vector<MeshEntity*> mesh_entities;
	for (int i = 0; i < ENTITY_COUNT; ++i) {
		auto m_e = reinterpret_cast<MeshEntity*>(entity_manager.entities[i]);
		if (m_e == nullptr ||
			!entityInherits(m_e->type, MESH_ENTITY) || m_e->mesh == nullptr || !m_e->do_lightmap ||
			entityInherits(m_e->type, ANIMATED_MESH_ENTITY) || entityInherits(m_e->type, VEGETATION_ENTITY))
			continue;

		float* img = (float*)malloc(w * h * c * sizeof(float));
		assert(img != NULL);
		lightmaps.push_back(img);
		mesh_entities.push_back(m_e);
	}

	RenderQueue q;
	createRenderQueue(q, entity_manager, true);

	constexpr int bounces = 2;
	for (int b = 0; b < bounces; b++) {
		std::cout << "Bounce " << b << " / " << bounces - 1 << "\n";

		int count = 1;
		// render all static geometry to their lightmaps
		for(int i = 0; i < lightmaps.size(); ++i) {
			const auto& img = lightmaps[i];
			const auto& m_e = mesh_entities[i];

			// @note relies on 0 bits representing 0.0 in floating format which is true for almost all standards
			memset(img, 0, sizeof(float) * w * h * c); // clear lightmap to black
			lmSetTargetLightmap(ctx, img, w, h, c);

			auto g_model_rot_scl = glm::mat4_cast(m_e->rotation) * glm::mat4x4(m_e->scale);
			auto g_model_pos = glm::translate(glm::mat4x4(1.0), m_e->position);

			indicators::ProgressBar bar{ indicators::option::BarWidth{50}, indicators::option::Start{"["}, indicators::option::Fill{"="}, indicators::option::Lead{">"}, indicators::option::Remainder{" "}, indicators::option::End{" ]"}, indicators::option::PostfixText{""}, indicators::option::ForegroundColor{indicators::Color::white}, indicators::option::FontStyles{std::vector<indicators::FontStyle>{indicators::FontStyle::bold}} };
			bar.set_option(indicators::option::ShowPercentage{ true });
			
			createShadowMapForGeometry(q, camera, m_e->position);

			auto& mesh = m_e->mesh;
			for (int j = 0; j < mesh->num_submeshes; ++j) {
				auto model = g_model_pos * mesh->transforms[j] * g_model_rot_scl;

				assert(mesh->uvs != nullptr); // Mesh should be parameterized and packed
				lmSetGeometry(ctx, (float*)&model[0][0],
					LM_FLOAT, mesh->vertices,	sizeof(*mesh->vertices),
					LM_FLOAT, mesh->normals,	sizeof(*mesh->normals),
					LM_FLOAT, mesh->uvs,		sizeof(*mesh->uvs),
					mesh->draw_count[j], LM_UNSIGNED_INT, &mesh->indices[mesh->draw_start[j]]);

				double last_update_time = glfwGetTime();

				int vp[4];
				glm::mat4 view, proj;
				bar.set_option(indicators::option::PostfixText{ "Baking lightmap " + std::to_string(count) + " / " + std::to_string(lightmaps.size())});

				while (lmBegin(ctx, vp, &camera.view[0][0], &camera.projection[0][0])) {
					camera.position = glm::vec3(
						ctx->meshPosition.sample.position.x, 
						ctx->meshPosition.sample.position.y, 
						ctx->meshPosition.sample.position.z
					);

					// Used for debugging the hemicube's rendering
					//using namespace std::this_thread; // sleep_for, sleep_until
					//using namespace std::chrono; // nanoseconds, system_clock, seconds
					//bindBackbuffer();
					//if(vp[0] == 0 && vp[1] == 0)
					//	clearFramebuffer();
					//glViewport(vp[0], vp[1], vp[2], vp[3]);
					//drawEntitiesHdr(entity_manager, skybox, skybox_irradiance, skybox_specular, camera, true);
					//glfwSwapBuffers(window);
					//sleep_for(milliseconds(10));

					glViewport(vp[0], vp[1], vp[2], vp[3]);
					drawRenderQueue(q, skybox, skybox_irradiance, skybox_specular, camera);
					
					double time = glfwGetTime();
					if (time - last_update_time > 1.0) {
						last_update_time = time;
						bar.set_progress(lmProgress(ctx) * 100.0f * ((float)(j+1) / (float)mesh->num_submeshes));
					}

					lmEnd(ctx);
				}
				bar.set_progress(100);
			}
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

			lmImageSmooth(img, temp, w, h, c);
			lmImageDilate(temp, img, w, h, c);
			// @hardcoded gamma
			lmImagePower(img, w, h, c, 1.0f / 2.2f, 0x7); // gamma correct color channels

			// Generate a new texture seperate from the color texture (which is still stored by asset manager)
			Texture* lightmap;
			if (b == 0) {
				lightmap = asset_manager.createTexture(level_path + "." + std::to_string(i) + ".tga");
				glGenTextures(1, &lightmap->id);
			}
			lightmap->format = GL_RGB16F;
			lightmap->resolution = glm::ivec2(w, h);

			glBindTexture(GL_TEXTURE_2D, lightmap->id);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0, GL_RGBA, GL_FLOAT, img);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glGenerateMipmap(GL_TEXTURE_2D);
			lightmap->complete = true;

			// Make all materials unique to each submesh and add the lightmapped texture
			const auto& mesh = *m_e->mesh;
			for (uint64_t submesh_i = 0; submesh_i < mesh.num_submeshes; submesh_i++) {
				Material new_mat = mesh.materials[mesh.material_indices[submesh_i]];
				new_mat.type = (MaterialType::Type)(new_mat.type | MaterialType::LIGHTMAPPED);
				new_mat.textures[TextureSlot::GI] = lightmap;
				// @hardcoded IDK whether to use uniform bindings or just a static map, this will do for now
				if (new_mat.uniforms.find("ambient_mult") == new_mat.uniforms.end()) {
					new_mat.uniforms.emplace("ambient_mult", 1.0f);
				}

				m_e->overidden_materials[submesh_i] = new_mat;
			}

			bar.set_progress((float)(count) / (float)lightmaps.size());
			bar.set_option(indicators::option::PostfixText{ "Postprocessing " + std::to_string(count) + " / " + std::to_string(lightmaps.size()) });
			count++;

			// If we are on the last bound we want to save to disk
			if (b == bounces - 1) {
				indicators::ProgressBar bar{ indicators::option::BarWidth{50}, indicators::option::Start{"["}, indicators::option::Fill{"="}, indicators::option::Lead{">"}, indicators::option::Remainder{" "}, indicators::option::End{" ]"}, indicators::option::PostfixText{""}, indicators::option::ForegroundColor{indicators::Color::white}, indicators::option::FontStyles{std::vector<indicators::FontStyle>{indicators::FontStyle::bold}} };
				bar.set_option(indicators::option::ShowPercentage{ true });

				const auto& img = lightmaps[i];
				const auto& m_e = mesh_entities[i];

				lmImageSaveTGAf(lightmap->handle.c_str(), img, w, h, c, 1.0);

				bar.set_progress((float)(count) / (float)lightmaps.size());
				bar.set_option(indicators::option::PostfixText{ "Writing TGA " + std::to_string(count) + " / " + std::to_string(lightmaps.size()) });
				count++;
			}
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
	sun_color = old_sun_color;
	window_width = old_window_width;
	window_height = old_window_height;
	glViewport(0, 0, window_width, window_height);
	graphics::do_bloom = old_bloom;
	graphics::do_msaa = old_msaa;
	graphics::do_shadows = old_shadows;
	initHdrFbo();
	initShadowFbo();

	// Update shadow vp for correct camera
	auto camera_ptr = &Cameras::editor_camera;
	if (playing) {
		camera_ptr = &Cameras::game_camera;
	}
	else if (editor::use_level_camera) {
		camera_ptr = &Cameras::level_camera;
	}

	updateShadowVP(*camera_ptr);
}