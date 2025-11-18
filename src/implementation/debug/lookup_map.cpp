// SPDX-FileCopyrightText: (c) 2020 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include "vulkan_api.hpp"

module pragma.prosper.vulkan;

import :debug.lookup_map;

#undef max
#undef GetObject

class PR_EXPORT ObjectLookupHandler {
  public:
	struct DebugObjectHistoryInfo {
		std::string debugName;
		std::string backTrace;
		std::string destroyBackTrace;
	};
	void RegisterObject(void *vkPtr, prosper::ContextObject &obj, prosper::debug::ObjectType type)
	{
		auto backTrace = util::debug::get_formatted_stack_backtrace_string();
		std::scoped_lock lock {m_objectMutex};
		m_lookupTable[vkPtr] = {&obj, type, std::move(backTrace)};
	}
	void ClearObject(void *vkPtr)
	{
		std::scoped_lock lock {m_objectMutex};
		auto it = m_lookupTable.find(vkPtr);
		if(it != m_lookupTable.end()) {
			m_debugHistory[vkPtr] = {it->second.obj->GetDebugName(), it->second.backTrace, util::debug::get_formatted_stack_backtrace_string()};
			m_lookupTable.erase(it);
		}
	}
	prosper::ContextObject *GetObject(void *vkPtr, prosper::debug::ObjectType *outType = nullptr, std::string *optOutBacktrace = nullptr) const
	{
		std::scoped_lock lock {m_objectMutex};
		auto it = m_lookupTable.find(vkPtr);
		if(it == m_lookupTable.end())
			return nullptr;
		if(outType != nullptr)
			*outType = it->second.type;
		if(optOutBacktrace)
			*optOutBacktrace = it->second.backTrace;
		return it->second.obj;
	}
	template<class T>
	T *GetObject(void *vkPtr) const
	{
		return static_cast<T *>(vkPtr);
	}

	const DebugObjectHistoryInfo *FindHistoryInfo(void *vkPtr) const
	{
		auto it = m_debugHistory.find(vkPtr);
		return (it != m_debugHistory.end()) ? &it->second : nullptr;
	}
  private:
	struct DebugObjectInfo {
		prosper::ContextObject *obj;
		prosper::debug::ObjectType type;
		std::string backTrace;
	};
	std::unordered_map<void *, DebugObjectInfo> m_lookupTable = {};
	mutable std::mutex m_objectMutex;
	std::unordered_map<void *, DebugObjectHistoryInfo> m_debugHistory;
};

static std::unique_ptr<ObjectLookupHandler> s_lookupHandler = nullptr;
void prosper::debug::set_debug_mode_enabled(bool b)
{
	enable_debug_recorded_image_layout(b);
	if(b == false) {
		s_lookupHandler = nullptr;
		return;
	}
	s_lookupHandler = std::make_unique<ObjectLookupHandler>();
}
bool prosper::debug::is_debug_mode_enabled() { return s_lookupHandler != nullptr; }
void prosper::debug::register_debug_object(void *vkPtr, prosper::ContextObject &obj, ObjectType type)
{
	if(s_lookupHandler == nullptr)
		return;
	s_lookupHandler->RegisterObject(vkPtr, obj, type);
}
void prosper::debug::register_debug_shader_pipeline(void *vkPtr, const ShaderPipelineInfo &pipelineInfo)
{
	if(s_lookupHandler == nullptr)
		return;
	//s_lookupHandler->RegisterObject(vkPtr,std::make_shared<ShaderPipelineInfo>(pipelineInfo),ObjectType::Pipeline);
}
void prosper::debug::deregister_debug_object(void *vkPtr)
{
	if(s_lookupHandler == nullptr)
		return;
	s_lookupHandler->ClearObject(vkPtr);
}

void *prosper::debug::get_object(void *vkObj, ObjectType &type, std::string *optOutBacktrace)
{
	if(s_lookupHandler == nullptr)
		return nullptr;
	return s_lookupHandler->GetObject(vkObj, &type, optOutBacktrace);
}
prosper::VlkImage *prosper::debug::get_image(const vk::Image &vkImage) { return (s_lookupHandler != nullptr) ? s_lookupHandler->GetObject<VlkImage>(vkImage) : nullptr; }
prosper::VlkImageView *prosper::debug::get_image_view(const vk::ImageView &vkImageView) { return (s_lookupHandler != nullptr) ? s_lookupHandler->GetObject<VlkImageView>(vkImageView) : nullptr; }
prosper::VlkSampler *prosper::debug::get_sampler(const vk::Sampler &vkSampler) { return (s_lookupHandler != nullptr) ? s_lookupHandler->GetObject<VlkSampler>(vkSampler) : nullptr; }
prosper::VlkBuffer *prosper::debug::get_buffer(const vk::Buffer &vkBuffer) { return (s_lookupHandler != nullptr) ? s_lookupHandler->GetObject<VlkBuffer>(vkBuffer) : nullptr; }
prosper::VlkCommandBuffer *prosper::debug::get_command_buffer(const vk::CommandBuffer &vkBuffer) { return (s_lookupHandler != nullptr) ? s_lookupHandler->GetObject<VlkCommandBuffer>(vkBuffer) : nullptr; }
prosper::VlkRenderPass *prosper::debug::get_render_pass(const vk::RenderPass &vkBuffer) { return (s_lookupHandler != nullptr) ? s_lookupHandler->GetObject<VlkRenderPass>(vkBuffer) : nullptr; }
prosper::VlkFramebuffer *prosper::debug::get_framebuffer(const vk::Framebuffer &vkBuffer) { return (s_lookupHandler != nullptr) ? s_lookupHandler->GetObject<VlkFramebuffer>(vkBuffer) : nullptr; }
prosper::VlkDescriptorSetGroup *prosper::debug::get_descriptor_set_group(const vk::DescriptorSet &vkBuffer) { return (s_lookupHandler != nullptr) ? s_lookupHandler->GetObject<VlkDescriptorSetGroup>(vkBuffer) : nullptr; }
prosper::debug::ShaderPipelineInfo *prosper::debug::get_shader_pipeline(const vk::Pipeline &vkPipeline) { return (s_lookupHandler != nullptr) ? s_lookupHandler->GetObject<prosper::debug::ShaderPipelineInfo>(vkPipeline) : nullptr; }

namespace prosper {
	namespace debug {
		static std::string object_type_to_string(ObjectType type)
		{
			switch(type) {
			case ObjectType::Image:
				return "Image";
			case ObjectType::ImageView:
				return "ImageView";
			case ObjectType::Sampler:
				return "Sampler";
			case ObjectType::Buffer:
				return "Buffer";
			case ObjectType::CommandBuffer:
				return "CommandBuffer";
			case ObjectType::RenderPass:
				return "RenderPass";
			case ObjectType::Framebuffer:
				return "Framebuffer";
			case ObjectType::DescriptorSet:
				return "DescriptorSet";
			case ObjectType::Pipeline:
				return "Pipeline";
			default:
				return "Invalid";
			}
		}
	};
};
void prosper::VlkContext::AddDebugObjectInformation(std::string &msgValidation)
{
	if(s_lookupHandler == nullptr)
		return;
	const std::string hexDigits = "0123456789abcdefABCDEF";
	auto prevPos = 0ull;
	auto pos = msgValidation.find("0x");
	auto posEnd = msgValidation.find_first_not_of(hexDigits, pos + 2u);

	prosper::VlkCommandBuffer *cmdBuffer = nullptr;
	std::stringstream r;

	struct DebugInfo {
		std::string debugName;
		std::string backTrace;
		std::string destroyBackTrace;
	};
	std::unordered_map<void *, DebugInfo> debugInfoMap;

	while(pos != std::string::npos && posEnd != std::string::npos) {
		auto strHex = msgValidation.substr(pos, posEnd - pos);
		auto hex = ::umath::to_hex_number(strHex);
		debug::ObjectType type;
		std::string backTrace;
		std::string destroyBackTrace;
		auto *o = s_lookupHandler->GetObject(reinterpret_cast<void *>(hex), &type, &backTrace);
		r << msgValidation.substr(prevPos, posEnd - prevPos);
		std::optional<std::string> debugName {};
		prosper::ContextObject *contextObject = nullptr;
		if(!o) {
			auto *historyInfo = s_lookupHandler->FindHistoryInfo(reinterpret_cast<void *>(hex));
			if(historyInfo) {
				debugName = historyInfo->debugName;
				backTrace = historyInfo->backTrace;
				destroyBackTrace = historyInfo->destroyBackTrace;
			}
		}
		else {
			switch(type) {
			case debug::ObjectType::Image:
				contextObject = dynamic_cast<prosper::VlkImage *>(o);
				break;
			case debug::ObjectType::ImageView:
				contextObject = dynamic_cast<prosper::VlkImageView *>(o);
				break;
			case debug::ObjectType::Sampler:
				contextObject = dynamic_cast<prosper::VlkSampler *>(o);
				break;
			case debug::ObjectType::Buffer:
				contextObject = dynamic_cast<prosper::VlkBuffer *>(o);
				break;
			case debug::ObjectType::CommandBuffer:
				if(cmdBuffer == nullptr)
					cmdBuffer = dynamic_cast<prosper::VlkCommandBuffer *>(o);
				contextObject = dynamic_cast<prosper::VlkCommandBuffer *>(o);
				break;
			case debug::ObjectType::RenderPass:
				contextObject = dynamic_cast<prosper::VlkRenderPass *>(o);
				break;
			case debug::ObjectType::Framebuffer:
				contextObject = dynamic_cast<prosper::VlkFramebuffer *>(o);
				break;
			case debug::ObjectType::DescriptorSet:
				contextObject = dynamic_cast<prosper::VlkDescriptorSetGroup *>(o);
				break;
			case debug::ObjectType::Pipeline:
				{
					//auto *info = static_cast<debug::ShaderPipelineInfo*>(o);
					//auto *str = static_cast<debug::ShaderPipelineInfo*>(o)->shader->GetDebugName(info->pipelineIdx);
					//debugName = (str != nullptr) ? *str : "shader_unknown";
					break;
				}
			default:
				break;
			}

			if(contextObject != nullptr)
				debugName = contextObject->GetDebugName();
		}

		if(debugName || !backTrace.empty() || !destroyBackTrace.empty())
			debugInfoMap[reinterpret_cast<void *>(hex)] = {debugName ? *debugName : std::string {}, backTrace, destroyBackTrace};

		if(debugName.has_value() && !debugName->empty())
			r << " (" << *debugName << ")";
		else if(contextObject == nullptr)
			r << " (Unknown)";
		else
			r << " (" << object_type_to_string(type) << ")";
		r << " prosper::ContextObject(0x" << o << ")";
		auto *dbgO = dynamic_cast<VlkDebugObject *>(contextObject);
		if(dbgO) {
			auto dbgIdx = dbgO->GetDebugIndex();
			if(dbgIdx != std::numeric_limits<size_t>::max())
				r << " (" << dbgIdx << ")";
		}

		prevPos = posEnd;
		pos = msgValidation.find("0x", pos + 1ull);
		posEnd = msgValidation.find_first_not_of(hexDigits, pos + 2u);
	}
	r << msgValidation.substr(prevPos);
	msgValidation = r.str();

	for(auto &[o, info] : debugInfoMap) {
		msgValidation += ::util::LOG_NL;
		msgValidation += "Object 0x" + ::umath::to_hex_string(reinterpret_cast<uint64_t>(o)) + ": ";
		if(info.debugName.empty() == false)
			msgValidation += "Debug name: " + info.debugName + ::util::LOG_NL;
		if(info.backTrace.empty() == false) {
			auto backtrace = "\t" + info.backTrace;
			ustring::replace(backtrace, "\n", ::util::LOG_NL + "\t");
			msgValidation += "Creation Backtrace:" + ::util::LOG_NL + backtrace + ::util::LOG_NL;
		}
		if(info.destroyBackTrace.empty() == false) {
			auto backtrace = "\t" + info.destroyBackTrace;
			ustring::replace(backtrace, "\n", ::util::LOG_NL + "\t");
			msgValidation += "Destruction Backtrace:" + ::util::LOG_NL + backtrace + ::util::LOG_NL;
		}
	}

	auto posPipeline = msgValidation.find("pipeline");
	if(posPipeline == std::string::npos)
		return;
	msgValidation += ".";

	const auto fPrintBoundPipeline = [&msgValidation](prosper::ICommandBuffer &cmd) {
		auto pipelineIdx = 0u;
		auto *shader = prosper::Shader::GetBoundPipeline(cmd, pipelineIdx);
		if(shader != nullptr)
			msgValidation += shader->GetIdentifier() + "; Pipeline: " + std::to_string(pipelineIdx) + ".";
		else
			msgValidation += "Unknown.";
	};
	if(cmdBuffer != nullptr) {
		auto &dbgName = cmdBuffer->GetDebugName();
		msgValidation += "Currently bound shader/pipeline for command buffer \"" + ((dbgName.empty() == false) ? dbgName : "Unknown") + "\": ";
		fPrintBoundPipeline(*cmdBuffer);
		return;
	}
	// We don't know which command buffer and/or shader/pipeline the message is referring to; Print info about ALL currently bound pipelines
	/*auto &boundPipelines = prosper::Shader::GetBoundPipelines();
	if(boundPipelines.empty())
		return;
	msgValidation += " Currently bound shaders/pipelines:";
	for(auto &boundPipeline : boundPipelines)
	{
		msgValidation += ::util::LOG_NL;
		auto &cmd = *boundPipeline.first;
		auto &dbgName = cmd.GetDebugName();
		msgValidation += ((dbgName.empty() == false) ? dbgName : "Unknown") +": ";
		fPrintBoundPipeline(*boundPipeline.first);
	}*/
}
