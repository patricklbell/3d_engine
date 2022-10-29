﻿// Include GLEW
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
#include "texture.hpp"
#include "utilities.hpp"
#include "shader.hpp"

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

bool runLightmapper(EntityManager& entity_manager, AssetManager &asset_manager, Texture* skybox, Texture *skybox_irradiance, Texture *skybox_specular) {
	lm_context* ctx = lmCreate(
		64,               // hemicube rendering resolution/quality
		0.01f, 100.0f,    // zNear, zFar
		1.0f, 1.0f, 1.0f, // sky/clear color
		2, 0.01f,         // hierarchical selective interpolation for speedup (passes, threshold)
		0.01f);           // modifier for camera-to-surface distance for hemisphere rendering.
						  // tweak this to trade-off between interpolated vertex normal quality and other artifacts (see declaration).
	if (!ctx) {
		std::cerr << "Could not initialize lightmapper.\n";
		return false;
	}

	constexpr int w = 654; //image height
	constexpr int h = 654; //image height
	constexpr int c = 4;

	// Buffer used during post processing
	float* temp = (float*)calloc((size_t)w * h * c, sizeof(float));

	auto old_window_width = window_width;
	auto old_window_height = window_height;
	auto old_bloom = graphics::do_bloom;
	auto old_shadows = graphics::do_shadows;
	auto old_msaa = graphics::do_msaa;

	graphics::do_bloom = false;
	graphics::do_msaa = false;
	graphics::do_shadows = false; // @todo static shadow map
	window_width = w;
	window_height = h;
	initHdrFbo();

	Camera camera;
	camera.near_plane = 0.001f;
	camera.far_plane = 100.0f;

	std::cout << "====================================================================================\n"
				 "\tBaking " << level_path << "'s Lightmap\n"
				 "====================================================================================\n";

	// Set all ambient occlusion to 0 intially
	for (int i = 0; i < ENTITY_COUNT; ++i) {
		auto m_e = reinterpret_cast<MeshEntity*>(entity_manager.entities[i]);
		if (entity_manager.entities[i] == nullptr || !entityInherits(m_e->type, MESH_ENTITY) || m_e->mesh == nullptr || entityInherits(m_e->type, ANIMATED_MESH_ENTITY)) continue;

		m_e->lightmap = asset_manager.getColorTexture(glm::vec3(0, 0, 0), GL_RGB);
		m_e->lightmap->handle = level_path + "." + std::to_string(i) + ".tga";
	}

	std::vector<float*> lightmaps;
	std::vector<MeshEntity*> mesh_entities;
	for (int i = 0; i < ENTITY_COUNT; ++i) {
		auto m_e = reinterpret_cast<MeshEntity*>(entity_manager.entities[i]);
		if (entity_manager.entities[i] == nullptr ||
			!entityInherits(m_e->type, MESH_ENTITY) || m_e->mesh == nullptr ||
			entityInherits(m_e->type, ANIMATED_MESH_ENTITY) || entityInherits(m_e->type, VEGETATION_ENTITY))
			continue;

		float* img = (float*)malloc((size_t)w * h * c * sizeof(float));
		assert(img != NULL);
		lightmaps.push_back(img);
		mesh_entities.push_back(m_e);
	}

	constexpr int bounces = 1;
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
			
			auto& mesh = m_e->mesh;
			for (int j = 0; j < mesh->num_meshes; ++j) {
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
					using namespace std::this_thread; // sleep_for, sleep_until
					using namespace std::chrono; // nanoseconds, system_clock, seconds
					bindBackbuffer();
					if(vp[0] == 0 && vp[1] == 0)
						clearFramebuffer();
					glViewport(vp[0], vp[1], vp[2], vp[3]);
					drawEntitiesHdr(entity_manager, skybox, skybox_irradiance, skybox_specular, camera, true);
					glfwSwapBuffers(window);
					sleep_for(milliseconds(10));

					glViewport(vp[0], vp[1], vp[2], vp[3]);
					drawEntitiesHdr(entity_manager, skybox, skybox_irradiance, skybox_specular, camera, true);
					
					double time = glfwGetTime();
					if (time - last_update_time > 1.0) {
						last_update_time = time;
						bar.set_progress(lmProgress(ctx) * 100.0f * ((float)(j+1) / (float)mesh->num_meshes));
					}

					lmEnd(ctx);
				}
				bar.set_progress(100);
			}
			count++;
		}

		indicators::ProgressBar bar{ indicators::option::BarWidth{50}, indicators::option::Start{"["}, indicators::option::Fill{"="}, indicators::option::Lead{">"}, indicators::option::Remainder{" "}, indicators::option::End{" ]"}, indicators::option::PostfixText{""}, indicators::option::ForegroundColor{indicators::Color::white}, indicators::option::FontStyles{std::vector<indicators::FontStyle>{indicators::FontStyle::bold}} };
		bar.set_option(indicators::option::ShowPercentage{ true });

		count = 1;
		// postprocess and upload all lightmaps to the GPU now to use them for indirect lighting during the next bounce.
		for (int i = 0; i < lightmaps.size(); ++i) {
			const auto& img = lightmaps[i];
			const auto& m_e = mesh_entities[i];

			lmImageSmooth(img, temp, w, h, c);
			lmImageDilate(temp, img, w, h, c);
			// @hardcoded gamma
			lmImagePower(img, w, h, c, 1.0f / 1.6f, 0x7); // gamma correct color channels

			//if (b == bounces - 1) {
			//	lmImagePower(img, w, h, c, 0.8); // Add some brightness to final result
			//}

			if (m_e->lightmap == nullptr) {
				m_e->lightmap = asset_manager.createTexture(level_path + "." + std::to_string(i) + ".tga");
			}
			if (m_e->lightmap->id == GL_FALSE) {
				glGenTextures(1, &m_e->lightmap->id);
			}
			m_e->lightmap->format = GL_RGBA;
			m_e->lightmap->resolution = glm::ivec2(w, h);

			glBindTexture(GL_TEXTURE_2D, m_e->lightmap->id);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0, GL_RGBA, GL_FLOAT, img);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glGenerateMipmap(GL_TEXTURE_2D);

			bar.set_progress((float)(count) / (float)lightmaps.size());
			bar.set_option(indicators::option::PostfixText{ "Postprocessing " + std::to_string(count) + " / " + std::to_string(lightmaps.size()) });
			count++;
		}
		glBindTexture(GL_TEXTURE_2D, 0);
		bar.set_progress(100);
	}

	lmDestroy(ctx);

	indicators::ProgressBar bar{ indicators::option::BarWidth{50}, indicators::option::Start{"["}, indicators::option::Fill{"="}, indicators::option::Lead{">"}, indicators::option::Remainder{" "}, indicators::option::End{" ]"}, indicators::option::PostfixText{""}, indicators::option::ForegroundColor{indicators::Color::white}, indicators::option::FontStyles{std::vector<indicators::FontStyle>{indicators::FontStyle::bold}} };
	bar.set_option(indicators::option::ShowPercentage{ true });

	// gamma correct and save lightmaps to disk
	int count = 1;
	for (int i = 0; i < lightmaps.size(); ++i) {
		const auto& img = lightmaps[i];
		const auto& m_e = mesh_entities[i];

		lmImageSaveTGAf(m_e->lightmap->handle.c_str(), img, w, h, c);

		bar.set_progress((float)(count) / (float)lightmaps.size());
		bar.set_option(indicators::option::PostfixText{ "Writing TGA " + std::to_string(count) + " / " + std::to_string(lightmaps.size()) });
		count++;
	}
	bar.set_progress(100);

	for (auto& img : lightmaps) {
		free(img);
	}
	free(temp);

	// Restore global graphics state
	window_width = old_window_width;
	window_height = old_window_height;
	glViewport(0, 0, window_width, window_height);
	graphics::do_bloom = old_bloom;
	graphics::do_msaa = old_msaa;
	graphics::do_shadows = old_shadows;
	initHdrFbo();
}