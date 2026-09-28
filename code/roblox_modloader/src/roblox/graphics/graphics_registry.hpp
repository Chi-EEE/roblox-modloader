#pragma once

#include "RobloxModLoader/roblox/graphics/render_pass.hpp"
#include "RobloxModLoader/roblox/graphics/types.hpp"
#include "RobloxModLoader/roblox/graphics/visual_engine.hpp"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

namespace rml::graphics
{
	void* adorn_render_pre_submit_pass_target();
	void* device_destructor_target();
	void* device_context_begin_pass_target();

	struct OpenPass
	{
		RBX::Graphics::Framebuffer* framebuffer{};
		unsigned store_mask{};
		unsigned flags{};
		bool resolves{};
		RBX::Graphics::PassResolve resolve{};
	};

	class GraphicsRegistry
	{
	public:
		static GraphicsRegistry& instance();

		[[nodiscard]] RBX::Graphics::VisualEngine* visual_engine() const;
		[[nodiscard]] RBX::Graphics::Device* device() const;
		[[nodiscard]] RBX::Graphics::SceneManager* scene_manager() const;
		void set_visual_engine(RBX::Graphics::VisualEngine* engine);
		void set_scene_manager(RBX::Graphics::SceneManager* scene_manager);
		void advance_frame();
		RenderCallbackId add_render_callback(RenderStage stage, RenderCallback callback);
		void remove_render_callback(RenderCallbackId id);
		std::size_t run_render_callbacks(RenderPassContext& context);
		[[nodiscard]] bool has_render_callbacks(RenderStage stage);
		unsigned on_begin_pass(RBX::Graphics::Framebuffer* framebuffer, unsigned store_mask, const RBX::Graphics::PassResolve* resolve, unsigned flags);
		[[nodiscard]] const OpenPass* open_pass(const RBX::Graphics::Framebuffer* framebuffer) const;
		void begin_scene(RBX::Graphics::SceneManager* scene_manager, bool engine_clouds, std::uint32_t capture_mode);
		RenderCallbackId add_device_teardown_callback(DeviceCallback callback);
		void set_device_teardown_available(bool available);
		void on_device_destroyed(RBX::Graphics::Device* device);
		void set_sky_stage_available(bool available);
		void set_sky_stage_enabled(bool enabled);
		void set_engine_clouds_replaced(bool replaced);
		[[nodiscard]] bool sky_stage_forced() const;
		[[nodiscard]] bool engine_clouds_enabled() const;
		[[nodiscard]] bool engine_clouds_replaced() const;
		[[nodiscard]] std::uint32_t capture_mode() const;
		void add_adorn_callback(AdornCallback callback);
		void run_adorn_callbacks(RBX::Graphics::AdornRender& adorn);
		[[nodiscard]] RBX::Graphics::AdornRender* adorn_render();
		[[nodiscard]] std::vector<RBX::Graphics::AdornRender*> adorn_renders();
		[[nodiscard]] bool validate();

	private:
		struct Entry
		{
			RenderCallbackId id;
			RenderStage stage;
			RenderCallback callback;
			unsigned failures;
		};

		struct AdornEntry
		{
			RBX::Graphics::AdornRender* adorn;
			std::uint64_t last_frame;
			float area;
		};

		void track_adorn_render(RBX::Graphics::AdornRender& adorn);

		std::atomic<RBX::Graphics::VisualEngine*> m_visual_engine{nullptr};
		std::atomic<RBX::Graphics::SceneManager*> m_scene_manager{nullptr};
		std::mutex m_callbacks_mutex;
		std::vector<Entry> m_callbacks;
		std::vector<std::pair<RenderCallbackId, DeviceCallback>> m_device_callbacks;
		std::vector<std::pair<AdornCallback, unsigned>> m_adorn_callbacks;
		std::atomic<std::uint64_t> m_frame{0};
		std::mutex m_adorn_mutex;
		std::vector<AdornEntry> m_adorn_renders;
		std::atomic<int> m_validation{0};
		std::atomic<RenderCallbackId> m_next_callback_id{1};
		std::atomic<bool> m_device_teardown_available{false};
		std::atomic<RBX::Graphics::Device*> m_device{nullptr};
		std::atomic<bool> m_sky_stage_available{false};
		std::atomic<bool> m_sky_stage_enabled{false};
		std::atomic<bool> m_engine_clouds{false};
		std::atomic<bool> m_engine_clouds_replaced{false};
		std::atomic<std::uint32_t> m_capture_mode{0};
		OpenPass m_open_pass;
		std::atomic<unsigned> m_post_opaque_callbacks{0};
	};
}
