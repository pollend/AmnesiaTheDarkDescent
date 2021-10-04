#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>
#include "graphics/HPLParameter.h"
#include "graphics/HPLMemberLayout.h"

static const hpl::HPLStructParameter TestUniformData[] = {

};

typedef struct {
  int x;
  int y;
  int z;
} TestStruct;

typedef struct  {
  TestStruct t1;
  TestStruct t2;
} TestLayout;

static const hpl::HPLShaderPermutation SupportPermutations[] = {
    {.bits = 0x1, .key = "UseReflection", .value = "TRUE"},
    {.bits = 0x2, .key = "UseCubeMapReflection", .value = "TRUE"},
    {.bits = 0x4, .key = "UseReflectionFading", .value = "TRUE"},
    {.bits = 0x8, .key = "UseFog", .value = "TRUE"}
};

static const hpl::HPLStructParameter TestUniformParameters[] = {
    {.type = hpl::HPL_PARAMETER_INT, .memberName = "x", .offset = offsetof(TestStruct, x)},
    {.type = hpl::HPL_PARAMETER_INT, .memberName = "y", .offset = offsetof(TestStruct, y)},
    {.type = hpl::HPL_PARAMETER_INT, .memberName = "z", .offset = offsetof(TestStruct, z)}
};

static const hpl::HPLMember TestLayoutMetaData[] = {
    {.memberName = "water.vert", .type = hpl::HPL_MEMBER_VERTEX_SHADER, .member_shader = { .permutations = absl::Span<const hpl::HPLShaderPermutation>(SupportPermutations, ARRAY_LEN(SupportPermutations))}},
    {.memberName = "water.frag", .type = hpl::HPL_MEMBER_VERTEX_SHADER, .member_shader = { .permutations = absl::Span<const hpl::HPLShaderPermutation>(SupportPermutations, ARRAY_LEN(SupportPermutations))}},
    {.memberName = "test_1",
     .type = hpl::HPL_MEMBER_STRUCT,
     .member_struct = {
         .offset = offsetof(TestLayout, t1),
         .size = sizeof(TestStruct),
         .parameters = absl::Span<const hpl::HPLStructParameter>(TestUniformParameters, ARRAY_LEN(TestUniformParameters))}},
    {.memberName = "test_2",
     .type = hpl::HPL_MEMBER_STRUCT,
     .member_struct = {
         .offset = offsetof(TestLayout, t2),
         .size = sizeof(TestStruct),
         .parameters = absl::Span<const hpl::HPLStructParameter>(TestUniformParameters, ARRAY_LEN(TestUniformParameters))}}
};


TEST_CASE( "testing layout configuration", "[unit]" ) {
  auto layout = hpl::HPLMemberLayout<TestLayout>(
      hpl::HPLMemberSpan(TestLayoutMetaData, ARRAY_LEN(TestLayoutMetaData)));

  SECTION("test modify field in struct") {
    TestLayout testField = {};
    int *value = nullptr;
    if (layout.getParameter<hpl::HPL_PARAMETER_INT>(&testField, "test_1", "x",
                                                    &value)) {
      (*value) = 10;
    }
    REQUIRE(testField.t1.x == 10);
    REQUIRE(testField.t2.x == 0);
    REQUIRE(testField.t1.y == 0);
  }
}
