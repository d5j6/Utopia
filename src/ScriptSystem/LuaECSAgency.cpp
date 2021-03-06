#include "LuaECSAgency.h"

#include <Utopia/ScriptSystem/LuaCtxMngr.h>
#include <Utopia/ScriptSystem/LuaContext.h>

using namespace Ubpa::Utopia;

const Ubpa::UECS::SystemFunc* LuaECSAgency::RegisterEntityJob(
	UECS::Schedule* s,
	sol::function systemFunc,
	std::string name,
	UECS::ArchetypeFilter filter,
	UECS::CmptLocator cmptLocator,
	UECS::SingletonLocator singletonLocator,
	bool isParallel
) {
	assert(!cmptLocator.CmptAccessTypes().empty());
	auto bytes = systemFunc.dump();
	auto sysfunc = s->RegisterChunkJob(
	[bytes = std::move(bytes), cmptLocator = std::move(cmptLocator)]
	(UECS::World* w, UECS::SingletonsView singletonsView, UECS::ChunkView chunk) {
		if (chunk.EntityNum() == 0)
			return;

		auto luaCtx = LuaCtxMngr::Instance().GetContext(w);
		auto L = luaCtx->Request();
		{
			sol::state_view lua(L);
			sol::function f = lua.load(bytes.as_string_view());

			auto arrayEntity = chunk.GetCmptArray<UECS::Entity>();
			std::vector<void*> cmpts;
			std::vector<UECS::CmptAccessType> types;
			std::vector<UECS::CmptAccessPtr> cmptPtrs;
			std::vector<size_t> sizes;
			cmpts.reserve(cmptLocator.CmptAccessTypes().size());
			types.reserve(cmptLocator.CmptAccessTypes().size());
			cmptPtrs.reserve(cmptLocator.CmptAccessTypes().size());
			sizes.reserve(cmptLocator.CmptAccessTypes().size());
			for (const auto& t : cmptLocator.CmptAccessTypes()) {
				cmpts.push_back(chunk.GetCmptArray(t));
				types.push_back(t);
				cmptPtrs.emplace_back(t, cmpts.back());
				sizes.push_back(w->entityMngr.cmptTraits.Sizeof(t));
			}

			size_t i = 0;
			do {
				UECS::CmptsView view{ cmptPtrs.data(), cmptPtrs.size() };
				f.call(w, singletonsView, arrayEntity[i], i, view);
				for (size_t j = 0; j < cmpts.size(); j++) {
					cmpts[j] = (reinterpret_cast<uint8_t*>(cmpts[j]) + sizes[j]);
					cmptPtrs[j] = { types[j], cmpts[j] };
				}
			} while (++i < chunk.EntityNum());
		}
		luaCtx->Recycle(L);
	}, std::move(name), std::move(filter), isParallel, std::move(singletonLocator));
	return sysfunc;
}

const Ubpa::UECS::SystemFunc* LuaECSAgency::RegisterChunkJob(
	UECS::Schedule* s,
	sol::function systemFunc,
	std::string name,
	UECS::ArchetypeFilter filter,
	UECS::SingletonLocator singletonLocator,
	bool isParallel
) {
	auto bytes = systemFunc.dump();
	auto sysfunc = s->RegisterChunkJob(
		[bytes](UECS::World* w, UECS::SingletonsView singletonsView, size_t entityBeginIndexInQuery, UECS::ChunkView chunk) {
			auto luaCtx = LuaCtxMngr::Instance().GetContext(w);
			auto L = luaCtx->Request();
			{
				sol::state_view lua(L);
				sol::function f = lua.load(bytes.as_string_view());
				f.call(w, singletonsView, entityBeginIndexInQuery, chunk);
			}
			luaCtx->Recycle(L);
		},
		std::move(name), std::move(filter), isParallel, std::move(singletonLocator)
	);
	return sysfunc;
}

const Ubpa::UECS::SystemFunc* LuaECSAgency::RegisterJob(
	UECS::Schedule* s,
	sol::function systemFunc,
	std::string name,
	UECS::SingletonLocator singletonLocator
) {
	auto bytes = systemFunc.dump();
	auto sysfunc = s->RegisterJob([bytes](UECS::World* w, UECS::SingletonsView singletonsView) {
		auto luaCtx = LuaCtxMngr::Instance().GetContext(w);
		auto L = luaCtx->Request();
		{
			sol::state_view lua(L);
			sol::function f = lua.load(bytes.as_string_view());
			f.call(w, singletonsView);
		}
		luaCtx->Recycle(L);
	}, std::move(name), std::move(singletonLocator));
	return sysfunc;
}
