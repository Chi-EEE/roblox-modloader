#include "cloud_renderer.hpp"

#include "cloud_registry.hpp"
#include "shaders_embedded.hpp"

#include <RobloxModLoader/roblox/graphics/device_context.hpp>
#include <RobloxModLoader/roblox/graphics/scene_manager.hpp>
#include <RobloxModLoader/roblox/graphics/shader_source.hpp>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace clouds
{
	using namespace RBX::Graphics;

	static constexpr unsigned k_frame_slot = 2;
	static constexpr unsigned k_frame_size = sizeof(CloudFrame);
	static constexpr float k_planet_radius = 2.27e7f;
	static constexpr float k_extinction_per_stud = 0.045f;
	static constexpr float k_noise_frequency = 1.f / 165000.f;
	static constexpr float k_weather_frequency = 1.f / 150000.f;
	static constexpr float k_camera_cut_distance = 250.f;
	static constexpr Quality k_qualities[] = {{32, 3, 2, true, 0.35f}, {48, 4, 2, true, 0.35f}, {48, 4, 3, false, 0.12f}, {96, 6, 3, false, 0.1f}};
	static constexpr int k_pattern[4][2] = {{0, 0}, {1, 1}, {1, 0}, {0, 1}};

	static SamplerState linear_wrap()
	{
		return SamplerState::make(SamplerState::Filter_Linear, SamplerState::Address_Wrap);
	}

	static SamplerState point_clamp()
	{
		return SamplerState::make(SamplerState::Filter_Point, SamplerState::Address_Clamp);
	}

	static void set4(float (&out)[4], const float x, const float y, const float z, const float w)
	{
		out[0] = x;
		out[1] = y;
		out[2] = z;
		out[3] = w;
	}

	static RBX::Color3 linear(const RBX::Color3& c)
	{
		return RBX::Color3(c.r * c.r, c.g * c.g, c.b * c.b);
	}

	static RBX::Vector3 xyz(const RBX::Vector4& v)
	{
		return RBX::Vector3(v.x, v.y, v.z);
	}

	static Matrix to_matrix(const RBX::Matrix4& source)
	{
		Matrix result{};
		const float* values = source;
		std::copy(values, values + 16, result.begin());
		return result;
	}

	static Matrix multiply(const Matrix& a, const Matrix& b)
	{
		Matrix result{};
		for (int row = 0; row < 4; ++row)
			for (int column = 0; column < 4; ++column)
				for (int k = 0; k < 4; ++k)
					result[row * 4 + column] += a[row * 4 + k] * b[k * 4 + column];
		return result;
	}

	static Matrix translation(const RBX::Vector3& v)
	{
		return {1, 0, 0, v.x, 0, 1, 0, v.y, 0, 0, 1, v.z, 0, 0, 0, 1};
	}

	static Matrix inverse(const Matrix& a)
	{
		std::array<double, 16> m{};
		for (std::size_t i = 0; i < 16; ++i)
			m[i] = a[i];

		std::array<double, 16> inv{};
		inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
		inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
		inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
		inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
		inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
		inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
		inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
		inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
		inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
		inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
		inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
		inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
		inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
		inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
		inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
		inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];

		const double determinant = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
		Matrix result{};
		if (std::abs(determinant) < 1e-30)
			return result;
		for (std::size_t i = 0; i < 16; ++i)
			result[i] = static_cast<float>(inv[i] / determinant);
		return result;
	}

	static std::shared_ptr<Texture> upload(Device& device, const NoiseTexture& noise, const Texture::Type type, const Texture::Format format, const std::string& name)
	{
		const auto mips = static_cast<unsigned>(noise.mips.size());
		auto texture = device.create_texture_impl(type, format, noise.size, noise.size, noise.depth, mips, 1, 1, Texture::Usage::ShaderRead, name);
		for (unsigned mip = 0; mip < mips; ++mip)
		{
			const auto size = std::max(noise.size >> mip, 1u);
			const auto depth = std::max(noise.depth >> mip, 1u);
			texture->upload(0, mip, TextureRegion{0, 0, 0, size, size, depth}, noise.mips[mip].data(), static_cast<unsigned>(noise.mips[mip].size()));
		}
		return texture;
	}

	static std::shared_ptr<Texture> target(Device& device, const Texture::Format format, const std::uint32_t width, const std::uint32_t height, const std::string& name)
	{
		const auto usage = static_cast<Texture::Usage>(static_cast<std::uint32_t>(Texture::Usage::ShaderRead) | static_cast<std::uint32_t>(Texture::Usage::RenderTarget));
		return device.create_texture_impl(Texture::Type::Type_2D, format, width, height, 1, 1, 1, 1, usage, name);
	}

	static std::shared_ptr<Framebuffer> framebuffer(Device& device, const std::vector<std::shared_ptr<Texture>>& colors, const std::string& name)
	{
		std::vector<Renderbuffer> color;
		for (const auto& texture : colors)
			color.push_back(Renderbuffer{texture, 0, 0, 0});
		return device.create_framebuffer_impl(color, Renderbuffer{}, name);
	}

	CloudRenderer::CloudRenderer(std::filesystem::path cache_file, std::shared_ptr<spdlog::logger> log) :
	    m_log(std::move(log))
	{
		m_noise_future = std::async(std::launch::async, [path = std::move(cache_file), log = m_log] {
			const auto start = std::chrono::steady_clock::now();
			auto noise = load_or_generate_noise(path);
			log->info("Noise volumes ready in {} ms", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count());
			return noise;
		});
	}

	std::shared_ptr<ShaderProgram> CloudRenderer::make_program(Device& device, const std::string_view defines, const std::string_view pass, const std::string_view entry, const std::string& name) const
	{
		std::string source;
		source.append(defines).append(shaders::common).append("\n").append(pass);
		auto program = rml::graphics::create_program(device, {{}, source, "FullscreenVS"}, {{}, source, entry}, name);
		if (!program)
			throw std::runtime_error(program.error());
		return std::move(*program);
	}

	void CloudRenderer::release()
	{
		m_programs = {};
		m_geometry.reset();
		m_layout.reset();
		m_shape.reset();
		m_detail.reset();
		m_weather.reset();
		m_targets = {};
		m_history_valid = false;
	}

	bool CloudRenderer::ensure_device(Device& device)
	{
		if (m_device == &device)
			return !m_failed;

		release();
		m_device = &device;
		m_failed = false;
		try
		{
			m_programs.depth = make_program(device, "", shaders::depth, "DepthPS", "rml_clouds_depth");
			m_programs.depth_msaa = make_program(device, "#define RML_MSAA 1\n", shaders::depth, "DepthPS", "rml_clouds_depth_msaa");
			m_programs.trace = make_program(device, "", shaders::trace, "TracePS", "rml_clouds_trace");
			m_programs.reconstruct = make_program(device, "", shaders::reconstruct, "ReconstructPS", "rml_clouds_reconstruct");
			m_programs.composite_sky = make_program(device, "#define RML_COMPOSITE_SKY 1\n", shaders::composite, "CompositePS", "rml_clouds_composite_sky");
			m_programs.composite_geometry = make_program(device, "#define RML_COMPOSITE_SKY 0\n", shaders::composite, "CompositePS", "rml_clouds_composite_geometry");
			m_layout = device.create_vertex_layout_impl({}, {}, "rml_clouds");
			m_geometry = device.create_geometry_impl(m_layout, nullptr, 0, nullptr, 0, "rml_clouds");
			m_log->info("Cloud programs ready (trace buffer mask 0x{:X}, texture mask 0x{:X})", m_programs.trace->buffer_mask, m_programs.trace->texture_mask);
		}
		catch (const std::exception& e)
		{
			m_log->error("Cloud renderer setup failed: {}", e.what());
			m_failed = true;
			release();
		}
		return !m_failed;
	}

	bool CloudRenderer::ensure_noise(Device& device)
	{
		if (m_shape)
			return true;
		if (!m_noise)
		{
			if (!m_noise_future.valid() || m_noise_future.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
				return false;
			m_noise = m_noise_future.get();
		}
		m_shape = upload(device, m_noise->shape, Texture::Type::Type_3D, Texture::Format::R8, "rml_clouds_shape");
		m_detail = upload(device, m_noise->detail, Texture::Type::Type_3D, Texture::Format::R8, "rml_clouds_detail");
		m_weather = upload(device, m_noise->weather, Texture::Type::Type_2D, Texture::Format::RG8, "rml_clouds_weather");
		return true;
	}

	void CloudRenderer::ensure_targets(Device& device, const std::uint32_t width, const std::uint32_t height, const bool checkerboard)
	{
		if (m_targets.width == width && m_targets.height == height && m_targets.checkerboard == checkerboard && m_targets.trace_fb)
			return;

		Targets targets;
		targets.width = width;
		targets.height = height;
		targets.checkerboard = checkerboard;
		targets.history_width = (width + 1) / 2;
		targets.history_height = (height + 1) / 2;
		targets.trace_width = checkerboard ? (targets.history_width + 1) / 2 : targets.history_width;
		targets.trace_height = checkerboard ? (targets.history_height + 1) / 2 : targets.history_height;
		targets.scene_distance = target(device, Texture::Format::R32F, targets.history_width, targets.history_height, "rml_clouds_scene_distance");
		targets.scene_distance_fb = framebuffer(device, {targets.scene_distance}, "rml_clouds_scene_distance");
		targets.trace_color = target(device, Texture::Format::RGBA16F, targets.trace_width, targets.trace_height, "rml_clouds_trace_color");
		targets.trace_distance = target(device, Texture::Format::R32F, targets.trace_width, targets.trace_height, "rml_clouds_trace_distance");
		targets.trace_fb = framebuffer(device, {targets.trace_color, targets.trace_distance}, "rml_clouds_trace");
		for (std::size_t i = 0; i < 2; ++i)
		{
			targets.history[i] = target(device, Texture::Format::RGBA16F, targets.history_width, targets.history_height, "rml_clouds_history");
			targets.history_fb[i] = framebuffer(device, {targets.history[i]}, "rml_clouds_history");
		}
		m_targets = std::move(targets);
		m_history_valid = false;
		m_log->info("Cloud targets {}x{} (trace {}x{}, history {}x{})", width, height, m_targets.trace_width, m_targets.trace_height, m_targets.history_width, m_targets.history_height);
	}

	void CloudRenderer::update_frame(const CloudSettings& settings, const GlobalShaderData& globals, const Quality& quality, const Texture* depth)
	{
		const auto now = std::chrono::steady_clock::now();
		const float dt = m_last_time ? std::clamp(std::chrono::duration<float>(now - *m_last_time).count(), 0.f, 0.1f) : 0.f;
		m_last_time = now;

		const RBX::Vector3 camera = xyz(globals.camera_position[0]);
		const RBX::Vector3 view_dir = xyz(globals.view_dir);
		const Matrix view_proj = multiply(to_matrix(globals.view_projection[0]), translation(camera));
		const bool cut = !m_prev_view_proj || (camera - m_prev_camera).length() > k_camera_cut_distance || view_dir.dot(m_prev_view_dir) < 0.5f;
		const Matrix prev_view_proj = m_prev_view_proj ? *m_prev_view_proj : view_proj;
		const RBX::Vector3 delta = m_prev_view_proj ? camera - m_prev_camera : RBX::Vector3(0, 0, 0);

		RBX::Vector3 wind(settings.wind_direction.x, 0.f, settings.wind_direction.z);
		wind = wind.length() > 1e-4f ? wind / wind.length() : RBX::Vector3(0, 0, 0);
		m_shape_offset[0] += wind.x * settings.wind_speed * dt;
		m_shape_offset[1] += wind.z * settings.wind_speed * dt;
		m_detail_offset[0] += wind.x * settings.wind_speed * 0.35 * dt;
		m_detail_offset[1] += wind.z * settings.wind_speed * 0.35 * dt;

		RBX::Vector3 sun = -xyz(globals.lamp0_dir);
		sun = sun.length() > 1e-4f ? sun / sun.length() : RBX::Vector3(0, 1, 0);
		const auto seed = static_cast<std::uint32_t>(settings.seed) * 2654435761u;
		const float seed_x = static_cast<float>(seed % 100003u) * 1.37f;
		const float seed_z = static_cast<float>((seed >> 16) % 100019u) * 1.91f;
		const RBX::Color3 albedo = linear(settings.color);
		const RBX::Vector4& sky_ambient = globals.ambient_color;
		const RBX::Vector4& fog = globals.fog_color;
		const float ambient = settings.ambient_intensity;
		const float sun_scale = settings.sun_intensity;
		const float ms = 0.1f + 0.75f * settings.multi_scattering;
		const float history_width = static_cast<float>(m_targets.history_width);
		const float history_height = static_cast<float>(m_targets.history_height);
		const float trace_width = static_cast<float>(m_targets.trace_width);
		const float trace_height = static_cast<float>(m_targets.trace_height);
		const float width = static_cast<float>(m_targets.width);
		const float height = static_cast<float>(m_targets.height);
		const auto& pattern = k_pattern[m_frame_index & 3];

		auto& f = m_frame;
		f.inv_view_proj = inverse(view_proj);
		f.prev_view_proj = prev_view_proj;
		f.prev_inv_view_proj = inverse(prev_view_proj);
		set4(f.camera_pos, camera.x, camera.y, camera.z, 0.5f / k_planet_radius);
		set4(f.camera_delta, delta.x, delta.y, delta.z, 0.f);
		set4(f.sun_dir, sun.x, sun.y, sun.z, 0.f);
		set4(f.sun_color, globals.lamp0_color.x * sun_scale, globals.lamp0_color.y * sun_scale, globals.lamp0_color.z * sun_scale, 1.f);
		set4(f.ambient_top, (sky_ambient.x + fog.x) * 0.18f * ambient, (sky_ambient.y + fog.y) * 0.18f * ambient, (sky_ambient.z + fog.z) * 0.18f * ambient, 0.f);
		set4(f.ambient_bottom, sky_ambient.x * 0.35f * ambient, sky_ambient.y * 0.35f * ambient, sky_ambient.z * 0.35f * ambient, 0.f);
		set4(f.fog_color, fog.x, fog.y, fog.z, settings.horizon_fade);
		set4(f.layer, settings.base_altitude, settings.base_altitude + settings.thickness, settings.thickness, 1.f / settings.thickness);
		set4(f.shape, settings.coverage, 2.f * settings.density * settings.density * k_extinction_per_stud, settings.shape_factor, settings.shape_scale * k_noise_frequency);
		set4(f.erosion, settings.erosion_factor, settings.erosion_scale * k_noise_frequency, settings.cloud_type, settings.powder);
		set4(f.wind, static_cast<float>(m_shape_offset[0]), static_cast<float>(m_shape_offset[1]), static_cast<float>(m_detail_offset[0]), static_cast<float>(m_detail_offset[1]));
		set4(f.weather, seed_x + static_cast<float>(m_shape_offset[0]), seed_z + static_cast<float>(m_shape_offset[1]), k_weather_frequency, ms);
		set4(f.albedo, albedo.r, albedo.g, albedo.b, 0.f);
		set4(f.trace_size, trace_width, trace_height, 1.f / trace_width, 1.f / trace_height);
		set4(f.history_size, history_width, history_height, 1.f / history_width, 1.f / history_height);
		set4(f.screen_size, width, height, 1.f / width, 1.f / height);
		set4(f.params, static_cast<float>(m_frame_index & 1023), static_cast<float>(quality.steps), static_cast<float>(quality.light_steps), static_cast<float>(quality.octaves));
		set4(f.temporal, m_history_valid && !cut ? 1.f : 0.f, quality.blend, quality.checkerboard ? static_cast<float>(pattern[0]) : 0.f, quality.checkerboard ? static_cast<float>(pattern[1]) : 0.f);
		const float depth_width = depth ? static_cast<float>(std::min(depth->width, m_targets.width)) : width;
		const float depth_height = depth ? static_cast<float>(std::min(depth->height, m_targets.height)) : height;
		set4(f.depth_info, depth_width, depth_height, std::max(settings.horizon_fade * 3.f, 50000.f), quality.checkerboard ? 1.f : 0.f);

		m_prev_view_proj = view_proj;
		m_prev_camera = camera;
		m_prev_view_dir = view_dir;
	}

	void CloudRenderer::draw(DeviceContext& context) const
	{
		context.draw(m_geometry.get(), Geometry::Primitive::Triangles, 0, 0, 3, 1, 0);
	}

	void CloudRenderer::run_depth(DeviceContext& context, Texture* depth)
	{
		const bool readable = depth && (depth->usage & static_cast<std::uint32_t>(Texture::Usage::ShaderRead)) != 0;
		PassClear clear{};
		clear.mask = PassClear::Color0;
		std::fill(std::begin(clear.color[0]), std::end(clear.color[0]), -1.f);
		context.begin_pass(m_targets.scene_distance_fb.get(), 0, PassClear::Color0, readable ? nullptr : &clear, nullptr, 0);
		if (readable)
		{
			context.set_render_state(RasterizerState::make(RasterizerState::Cull_None), BlendState::opaque(), DepthState::make(DepthState::Function_Always, false));
			context.bind_program((depth->samples > 1 ? m_programs.depth_msaa : m_programs.depth).get());
			context.bind_texture(0, depth, point_clamp());
			context.bind_buffer_data(k_frame_slot, &m_frame, k_frame_size);
			draw(context);
		}
		context.end_pass();
	}

	void CloudRenderer::run_trace(DeviceContext& context)
	{
		context.begin_pass(m_targets.trace_fb.get(), 0, PassClear::Color0 | PassClear::Color1, nullptr, nullptr, 0);
		context.set_render_state(RasterizerState::make(RasterizerState::Cull_None), BlendState::opaque(), DepthState::make(DepthState::Function_Always, false));
		context.bind_program(m_programs.trace.get());
		context.bind_texture(0, m_shape.get(), linear_wrap());
		context.bind_texture(1, m_detail.get(), linear_wrap());
		context.bind_texture(2, m_weather.get(), linear_wrap());
		context.bind_texture(3, m_targets.scene_distance.get(), point_clamp());
		context.bind_buffer_data(k_frame_slot, &m_frame, k_frame_size);
		draw(context);
		context.end_pass();
	}

	void CloudRenderer::run_reconstruct(DeviceContext& context)
	{
		const auto next = m_history_index ^ 1u;
		context.begin_pass(m_targets.history_fb[next].get(), 0, PassClear::Color0, nullptr, nullptr, 0);
		context.set_render_state(RasterizerState::make(RasterizerState::Cull_None), BlendState::opaque(), DepthState::make(DepthState::Function_Always, false));
		context.bind_program(m_programs.reconstruct.get());
		context.bind_texture(0, m_targets.trace_color.get(), point_clamp());
		context.bind_texture(1, m_targets.trace_distance.get(), point_clamp());
		context.bind_texture(2, m_targets.history[m_history_index].get(), point_clamp());
		context.bind_texture(3, m_targets.scene_distance.get(), point_clamp());
		context.bind_buffer_data(k_frame_slot, &m_frame, k_frame_size);
		draw(context);
		context.end_pass();
		m_history_index = next;
		m_history_valid = true;
	}

	void CloudRenderer::prepare(rml::graphics::RenderPassContext& pass)
	{
		m_ready = false;
		if (pass.capture_mode != 0 || !pass.device || !pass.context || !pass.scene_manager || !pass.globals)
			return;

		const auto settings = CloudRegistry::instance().active();
		if (!settings)
		{
			m_active_logged = false;
			return;
		}

		try
		{
			if (!ensure_device(*pass.device) || !ensure_noise(*pass.device))
				return;

			const auto* scene = pass.scene_manager;
			const auto width = scene->drs_width ? scene->drs_width : scene->view_width;
			const auto height = scene->drs_height ? scene->drs_height : scene->view_height;
			if (!width || !height)
				return;

			const auto& quality = k_qualities[std::clamp(settings->quality, 1, 4) - 1];
			ensure_targets(*pass.device, width, height, quality.checkerboard);
			update_frame(*settings, *pass.globals, quality, pass.scene_depth);

			auto& context = *pass.context;
			context.begin_group("VolumetricClouds", 0);
			run_depth(context, pass.scene_depth);
			run_trace(context);
			run_reconstruct(context);
			context.end_group();
			++m_frame_index;
			m_ready = true;

			if (!m_active_logged)
			{
				m_active_logged = true;
				const auto& g = *pass.globals;
				m_log->info("Clouds active: {}x{}, quality {}, depth samples {}", width, height, settings->quality, pass.scene_depth ? pass.scene_depth->samples : 0);
				m_log->info("Lighting: sun dir ({:.3f}, {:.3f}, {:.3f}) color ({:.3f}, {:.3f}, {:.3f}), outdoor ({:.3f}, {:.3f}, {:.3f}), ambient ({:.3f}, {:.3f}, {:.3f}), fog ({:.3f}, {:.3f}, {:.3f}) params ({:.1f}, {:.1f}, {:.1f}, {:.5f})",
				    g.lamp0_dir.x, g.lamp0_dir.y, g.lamp0_dir.z, g.lamp0_color.x, g.lamp0_color.y, g.lamp0_color.z, g.outdoor_ambient_color.x, g.outdoor_ambient_color.y, g.outdoor_ambient_color.z,
				    g.ambient_color.x, g.ambient_color.y, g.ambient_color.z, g.fog_color.x, g.fog_color.y, g.fog_color.z, g.fog_params.x, g.fog_params.y, g.fog_params.z, g.fog_params.w);
			}
		}
		catch (const std::exception& e)
		{
			m_log->error("Cloud frame failed: {}", e.what());
			m_failed = true;
			release();
		}
	}

	void CloudRenderer::composite(rml::graphics::RenderPassContext& pass)
	{
		if (!m_ready || !pass.context)
			return;
		m_ready = false;

		auto& context = *pass.context;
		const auto raster = RasterizerState::make(RasterizerState::Cull_None);
		const auto blend = BlendState::make(BlendState::Factor_SrcAlpha, BlendState::Factor_InvSrcAlpha, BlendState::Factor_Zero, BlendState::Factor_One);
		context.bind_texture(0, m_targets.history[m_history_index].get(), point_clamp());
		context.bind_buffer_data(k_frame_slot, &m_frame, k_frame_size);
		context.set_render_state(raster, blend, DepthState::make(DepthState::Function_GreaterEqual, false));
		context.bind_program(m_programs.composite_sky.get());
		draw(context);
		context.set_render_state(raster, blend, DepthState::make(DepthState::Function_Less, false));
		context.bind_program(m_programs.composite_geometry.get());
		draw(context);
	}
}
