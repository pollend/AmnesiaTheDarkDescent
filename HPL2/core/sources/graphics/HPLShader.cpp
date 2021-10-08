#include "graphics/HPLShader.h"

void hpl::IHPLShader::configureShader(Renderer* render,const HPLMember& member, HPLShaderType& shaderTypes, uint32_t permutation) {

    ShaderLoadDesc shaderDesc = {};
    absl::InlinedVector<ShaderMacro, 10> _macros[SHADER_STAGE_COUNT];
    size_t current_stage = 0; //stageCount++;
    for(auto& stage: member.member_shader.stages) {
      auto& descStage = shaderDesc.mStages[current_stage];
      current_stage++;
      for (const auto &perm : stage.permutations) {
        if (perm.bits == 0 || (perm.bits & permutation) > 0) {
          _macros[current_stage].push_back(
              {.definition = perm.key, .value = perm.value});
        }
      }
      descStage.pFileName = stage.shaderName;
      descStage.pEntryPointName = stage.entryPointName;
      descStage.pMacros = _macros[current_stage].data();
      descStage.mMacroCount = _macros[current_stage].size();
      Shader* shader = nullptr;
      shaderTypes.name = member.memberName;
      addShader(render, &shaderDesc, &shaderTypes.shader);
    }

}
void hpl::IHPLShader::configureRootSignature(const IHPLMemberLayout& layout, const absl::Span<Shader*>& shader) {

}
