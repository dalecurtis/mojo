// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shader_translator.h"

#include <string.h>
#include <GLES2/gl2.h>
#include <algorithm>

#include "base/at_exit.h"
#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"

namespace gpu {
namespace gles2 {

namespace {

class ShaderTranslatorInitializer {
 public:
  ShaderTranslatorInitializer() {
    TRACE_EVENT0("gpu", "ShInitialize");
    CHECK(ShInitialize());
  }

  ~ShaderTranslatorInitializer() {
    TRACE_EVENT0("gpu", "ShFinalize");
    ShFinalize();
  }
};

base::LazyInstance<ShaderTranslatorInitializer> g_translator_initializer =
    LAZY_INSTANCE_INITIALIZER;

void GetAttributes(ShHandle compiler, AttributeMap* var_map) {
  if (!var_map)
    return;
  var_map->clear();
  const std::vector<sh::Attribute>* attribs = ShGetAttributes(compiler);
  if (attribs) {
    for (size_t ii = 0; ii < attribs->size(); ++ii)
      (*var_map)[(*attribs)[ii].mappedName] = (*attribs)[ii];
  }
}

void GetUniforms(ShHandle compiler, UniformMap* var_map) {
  if (!var_map)
    return;
  var_map->clear();
  const std::vector<sh::Uniform>* uniforms = ShGetUniforms(compiler);
  if (uniforms) {
    for (size_t ii = 0; ii < uniforms->size(); ++ii)
      (*var_map)[(*uniforms)[ii].mappedName] = (*uniforms)[ii];
  }
}

void GetVaryings(ShHandle compiler, VaryingMap* var_map) {
  if (!var_map)
    return;
  var_map->clear();
  const std::vector<sh::Varying>* varyings = ShGetVaryings(compiler);
  if (varyings) {
    for (size_t ii = 0; ii < varyings->size(); ++ii)
      (*var_map)[(*varyings)[ii].mappedName] = (*varyings)[ii];
  }
}

void GetNameHashingInfo(ShHandle compiler, NameMap* name_map) {
  if (!name_map)
    return;
  name_map->clear();

  size_t hashed_names_count = 0;
  ShGetInfo(compiler, SH_HASHED_NAMES_COUNT, &hashed_names_count);
  if (hashed_names_count == 0)
    return;

  size_t name_max_len = 0, hashed_name_max_len = 0;
  ShGetInfo(compiler, SH_NAME_MAX_LENGTH, &name_max_len);
  ShGetInfo(compiler, SH_HASHED_NAME_MAX_LENGTH, &hashed_name_max_len);

  scoped_ptr<char[]> name(new char[name_max_len]);
  scoped_ptr<char[]> hashed_name(new char[hashed_name_max_len]);

  for (size_t i = 0; i < hashed_names_count; ++i) {
    ShGetNameHashingEntry(compiler, i, name.get(), hashed_name.get());
    (*name_map)[hashed_name.get()] = name.get();
  }
}

}  // namespace

ShaderTranslator::DestructionObserver::DestructionObserver() {
}

ShaderTranslator::DestructionObserver::~DestructionObserver() {
}

ShaderTranslator::ShaderTranslator()
    : compiler_(NULL),
      implementation_is_glsl_es_(false),
      driver_bug_workarounds_(static_cast<ShCompileOptions>(0)) {
}

bool ShaderTranslator::Init(
    GLenum shader_type,
    ShShaderSpec shader_spec,
    const ShBuiltInResources* resources,
    ShaderTranslatorInterface::GlslImplementationType glsl_implementation_type,
    ShCompileOptions driver_bug_workarounds) {
  // Make sure Init is called only once.
  DCHECK(compiler_ == NULL);
  DCHECK(shader_type == GL_FRAGMENT_SHADER || shader_type == GL_VERTEX_SHADER);
  DCHECK(shader_spec == SH_GLES2_SPEC || shader_spec == SH_WEBGL_SPEC);
  DCHECK(resources != NULL);

  g_translator_initializer.Get();

  ShShaderOutput shader_output =
      (glsl_implementation_type == kGlslES ? SH_ESSL_OUTPUT : SH_GLSL_OUTPUT);

  {
    TRACE_EVENT0("gpu", "ShConstructCompiler");
    compiler_ = ShConstructCompiler(
        shader_type, shader_spec, shader_output, resources);
  }
  compiler_options_ = *resources;
  implementation_is_glsl_es_ = (glsl_implementation_type == kGlslES);
  driver_bug_workarounds_ = driver_bug_workarounds;
  return compiler_ != NULL;
}

int ShaderTranslator::GetCompileOptions() const {
  int compile_options =
      SH_OBJECT_CODE | SH_VARIABLES | SH_ENFORCE_PACKING_RESTRICTIONS |
      SH_LIMIT_EXPRESSION_COMPLEXITY | SH_LIMIT_CALL_STACK_DEPTH |
      SH_CLAMP_INDIRECT_ARRAY_BOUNDS;

  compile_options |= driver_bug_workarounds_;

  return compile_options;
}

bool ShaderTranslator::Translate(const std::string& shader_source,
                                 std::string* info_log,
                                 std::string* translated_source,
                                 AttributeMap* attrib_map,
                                 UniformMap* uniform_map,
                                 VaryingMap* varying_map,
                                 NameMap* name_map) const {
  // Make sure this instance is initialized.
  DCHECK(compiler_ != NULL);

  bool success = false;
  {
    TRACE_EVENT0("gpu", "ShCompile");
    const char* const shader_strings[] = { shader_source.c_str() };
    success = !!ShCompile(
        compiler_, shader_strings, 1, GetCompileOptions());
  }
  if (success) {
    if (translated_source) {
      translated_source->clear();
      // Get translated shader.
      size_t obj_code_len = 0;
      ShGetInfo(compiler_, SH_OBJECT_CODE_LENGTH, &obj_code_len);
      if (obj_code_len > 1) {
        scoped_ptr<char[]> buffer(new char[obj_code_len]);
        ShGetObjectCode(compiler_, buffer.get());
        *translated_source = std::string(buffer.get(), obj_code_len - 1);
      }
    }
    // Get info for attribs, uniforms, and varyings.
    GetAttributes(compiler_, attrib_map);
    GetUniforms(compiler_, uniform_map);
    GetVaryings(compiler_, varying_map);
    // Get info for name hashing.
    GetNameHashingInfo(compiler_, name_map);
  }

  // Get info log.
  if (info_log) {
    info_log->clear();
    size_t info_log_len = 0;
    ShGetInfo(compiler_, SH_INFO_LOG_LENGTH, &info_log_len);
    if (info_log_len > 1) {
      scoped_ptr<char[]> buffer(new char[info_log_len]);
      ShGetInfoLog(compiler_, buffer.get());
      *info_log = std::string(buffer.get(), info_log_len - 1);
    }
  }

  return success;
}

std::string ShaderTranslator::GetStringForOptionsThatWouldAffectCompilation()
    const {
  DCHECK(compiler_ != NULL);

  size_t resource_len = 0;
  ShGetInfo(compiler_, SH_RESOURCES_STRING_LENGTH, &resource_len);
  DCHECK(resource_len > 1);
  scoped_ptr<char[]> resource_str(new char[resource_len]);

  ShGetBuiltInResourcesString(compiler_, resource_len, resource_str.get());

  return std::string(":CompileOptions:" +
         base::IntToString(GetCompileOptions())) +
         std::string(resource_str.get());
}

void ShaderTranslator::AddDestructionObserver(
    DestructionObserver* observer) {
  destruction_observers_.AddObserver(observer);
}

void ShaderTranslator::RemoveDestructionObserver(
    DestructionObserver* observer) {
  destruction_observers_.RemoveObserver(observer);
}

ShaderTranslator::~ShaderTranslator() {
  FOR_EACH_OBSERVER(DestructionObserver,
                    destruction_observers_,
                    OnDestruct(this));

  if (compiler_ != NULL)
    ShDestruct(compiler_);
}

}  // namespace gles2
}  // namespace gpu

