#pragma once

#include "cloud_settings.hpp"
#include "noise_volumes.hpp"

#include <RobloxModLoader/roblox/graphics/device.hpp>
#include <RobloxModLoader/roblox/graphics/global_shader_data.hpp>
#include <RobloxModLoader/roblox/graphics/render_pass.hpp>
#include <spdlog/spdlog.h>

#include <array>
#include <chrono>
#include <filesystem>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace clouds
{
	using Matrix = std::array<float, 16>;

	struct alignas(16) CloudFrame
	{
		Matrix inv_view_proj;
		Matrix prev_view_proj;
		Matrix prev_inv_view_proj;
		float camera_pos[4];
		float camera_delta[4];
		float sun_dir[4];
		float sun_color[4];
		float ambient_top[4];
		float ambient_bottom[4];
		float fog_color[4];
		float layer[4];
		float shape[4];
		float erosion[4];
		float wind[4];
		float weather[4];
		float albedo[4];
		float trace_size[4];
		float history_size[4];
		float screen_size[4];
		float params[4];
		float temporal[4];
		float depth_info[4];
	};

	static_assert(sizeof(CloudFrame) % 16 == 0);

	struct Quality
	{
		int steps;
		int light_steps;
		int octaves;
		bool checkerboard;
		float blend;
	};

	class CloudRenderer
	{
	public:
		CloudRenderer(std::filesystem::path cache_file, std::shared_ptr<spdlog::logger> log);

		void prepare(rml::graphics::RenderPassContext& pass);
		void composite(rml::graphics::RenderPassContext& pass);

	private:
		struct Programs
		{
			std::shared_ptr<RBX::Graphics::ShaderProgram> depth;
			std::shared_ptr<RBX::Graphics::ShaderProgram> depth_msaa;
			std::shared_ptr<RBX::Graphics::ShaderProgram> trace;
			std::shared_ptr<RBX::Graphics::ShaderProgram> reconstruct;
			std::shared_ptr<RBX::Graphics::ShaderProgram> composite_sky;
			std::shared_ptr<RBX::Graphics::ShaderProgram> composite_geometry;
		};

		struct Targets
		{
			std::uint32_t width{};
			std::uint32_t height{};
			bool checkerboard{};
			std::uint32_t history_width{};
			std::uint32_t history_height{};
			std::uint32_t trace_width{};
			std::uint32_t trace_height{};
			std::shared_ptr<RBX::Graphics::Texture> scene_distance;
			std::shared_ptr<RBX::Graphics::Framebuffer> scene_distance_fb;
			std::shared_ptr<RBX::Graphics::Texture> trace_color;
			std::shared_ptr<RBX::Graphics::Texture> trace_distance;
			std::shared_ptr<RBX::Graphics::Framebuffer> trace_fb;
			std::array<std::shared_ptr<RBX::Graphics::Texture>, 2> history;
			std::array<std::shared_ptr<RBX::Graphics::Framebuffer>, 2> history_fb;
		};

		bool ensure_device(RBX::Graphics::Device& device);
		bool ensure_noise(RBX::Graphics::Device& device);
		void ensure_targets(RBX::Graphics::Device& device, std::uint32_t width, std::uint32_t height, bool checkerboard);
		void update_frame(const CloudSettings& settings, const RBX::Graphics::GlobalShaderData& globals, const Quality& quality, const RBX::Graphics::Texture* depth);
		void run_depth(RBX::Graphics::DeviceContext& context, RBX::Graphics::Texture* depth);
		void run_trace(RBX::Graphics::DeviceContext& context);
		void run_reconstruct(RBX::Graphics::DeviceContext& context);
		void draw(RBX::Graphics::DeviceContext& context) const;
		std::shared_ptr<RBX::Graphics::ShaderProgram> make_program(RBX::Graphics::Device& device, std::string_view defines, std::string_view pass, std::string_view entry, const std::string& name) const;
		void release();

		std::shared_ptr<spdlog::logger> m_log;
		std::future<NoiseSet> m_noise_future;
		std::optional<NoiseSet> m_noise;
		RBX::Graphics::Device* m_device{};
		bool m_failed{};
		Programs m_programs;
		std::shared_ptr<RBX::Graphics::VertexLayout> m_layout;
		std::shared_ptr<RBX::Graphics::Geometry> m_geometry;
		std::shared_ptr<RBX::Graphics::Texture> m_shape;
		std::shared_ptr<RBX::Graphics::Texture> m_detail;
		std::shared_ptr<RBX::Graphics::Texture> m_weather;
		Targets m_targets;
		CloudFrame m_frame{};
		std::uint32_t m_history_index{};
		bool m_history_valid{};
		bool m_ready{};
		std::uint64_t m_frame_index{};
		std::optional<std::chrono::steady_clock::time_point> m_last_time;
		std::optional<Matrix> m_prev_view_proj;
		RBX::Vector3 m_prev_camera;
		RBX::Vector3 m_prev_view_dir;
		double m_shape_offset[2]{};
		double m_detail_offset[2]{};
		bool m_active_logged{};
	};
}
