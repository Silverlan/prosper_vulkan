/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "prosper_vulkan_definitions.hpp"
#include <vulkan/vulkan.h>
#include <vulkan/vulkan.hpp>
#include "stdafx_prosper_vulkan.h"
#include "debug/vk_debug_lookup_map.hpp"
#include "debug/prosper_debug.hpp"
#include "image/prosper_image.hpp"
#include "image/prosper_image_view.hpp"
#include "image/prosper_sampler.hpp"
#include "image/vk_image.hpp"
#include "image/vk_image_view.hpp"
#include "image/vk_sampler.hpp"
#include "debug/vk_debug_object.hpp"
#include "vk_context.hpp"
#include "vk_command_buffer.hpp"
#include "vk_render_pass.hpp"
#include "vk_framebuffer.hpp"
#include "vk_descriptor_set_group.hpp"
#include "buffers/vk_buffer.hpp"
#include "prosper_render_pass.hpp"
#include "prosper_framebuffer.hpp"
#include "prosper_command_buffer.hpp"
#include "prosper_descriptor_set_group.hpp"
#include "prosper_context_object.hpp"
#include "shader/prosper_shader.hpp"
#include <sharedutils/util.h>
#include <sharedutils/util_string.h>
#include <sstream>
#include <mutex>

#undef GetObject
class DLLPROSPER_VK ObjectLookupHandler {
  public:
	void RegisterObject(void *vkPtr, prosper::ContextObject &obj, prosper::debug::ObjectType type)
	{
		std::scoped_lock lock {m_objectMutex};
		m_lookupTable[vkPtr] = {&obj, type};
	}
	void ClearObject(void *vkPtr)
	{
		std::scoped_lock lock {m_objectMutex};
		auto it = m_lookupTable.find(vkPtr);
		if(it != m_lookupTable.end()) {
			m_nameHistory[vkPtr] = it->second.first->GetDebugName();
			m_lookupTable.erase(it);
		}
	}
	prosper::ContextObject *GetObject(void *vkPtr, prosper::debug::ObjectType *outType = nullptr) const
	{
		std::scoped_lock lock {m_objectMutex};
		auto it = m_lookupTable.find(vkPtr);
		if(it == m_lookupTable.end())
			return nullptr;
		if(outType != nullptr)
			*outType = it->second.second;
		return it->second.first;
	}
	template<class T>
	T *GetObject(void *vkPtr) const
	{
		return static_cast<T *>(vkPtr);
	}

	const std::string *FindHistoryName(void *vkPtr) const
	{
		auto it = m_nameHistory.find(vkPtr);
		return (it != m_nameHistory.end()) ? &it->second : nullptr;
	}
  private:
	std::unordered_map<void *, std::pair<prosper::ContextObject *, prosper::debug::ObjectType>> m_lookupTable = {};
	mutable std::mutex m_objectMutex;
	std::unordered_map<void *, std::string> m_nameHistory;
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

void *prosper::debug::get_object(void *vkObj, ObjectType &type)
{
	if(s_lookupHandler == nullptr)
		return nullptr;
	return s_lookupHandler->GetObject(vkObj, &type);
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
	while(pos != std::string::npos && posEnd != std::string::npos) {
		auto strHex = msgValidation.substr(pos, posEnd - pos);
		auto hex = ::util::to_hex_number(strHex);
		debug::ObjectType type;
		auto *o = s_lookupHandler->GetObject(reinterpret_cast<void *>(hex), &type);
		r << msgValidation.substr(prevPos, posEnd - prevPos);
		std::optional<std::string> debugName {};
		prosper::ContextObject *contextObject = nullptr;
		if(!o) {
			auto *p = s_lookupHandler->FindHistoryName(reinterpret_cast<void *>(hex));
			if(p)
				debugName = *p;
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
		}

		if(debugName.has_value())
			r << " (" << *debugName << ")";
		else if(contextObject == nullptr)
			r << " (Unknown)";
		else {
			auto &debugName = contextObject->GetDebugName();
			if(debugName.empty() == false)
				r << " (" << debugName << ")";
			else
				r << " (" << object_type_to_string(type) << ")";
		}
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
		msgValidation += "\n";
		auto &cmd = *boundPipeline.first;
		auto &dbgName = cmd.GetDebugName();
		msgValidation += ((dbgName.empty() == false) ? dbgName : "Unknown") +": ";
		fPrintBoundPipeline(*boundPipeline.first);
	}*/
}
