import argparse
from dataclasses import dataclass
import re
from typing import IO, Iterable, Sequence


def camelName_to_snake_name(word: str) -> str:
    return re.sub(r"([a-z])([A-Z])", r"\1_\2", word).lower()


assert camelName_to_snake_name("fooBar") == "foo_bar"
assert camelName_to_snake_name("fooBAR") == "foo_bar"


@dataclass(frozen=True)
class Type:
    """
    Type
    """

    name: str
    spelling: str
    alt_public_type: str | None = None
    copy_code: Sequence[str] | None = None
    destroy_code: Sequence[str] = ()

    def var_declspec(self, varname: str) -> str:
        return f"{self.spelling} {varname}"

    @property
    def public_type(self) -> str:
        return self.alt_public_type or self.spelling

    @property
    def copy_fn_name(self) -> str:
        return f"_copy_{self.name}"

    @property
    def destroy_fn_name(self) -> str:
        return f"_destroy_{self.name}"

    def get_copy_code(self) -> Iterable[str]:
        if self.copy_code is not None:
            yield from self.copy_code
            return
        yield f"// Default copy impl for {self.name}"
        yield "*dst = *src;"


@dataclass(frozen=True)
class Parameter:
    """
    Parameter
    """

    name: str
    type: Type
    deprecation_message: str | None = None

    @property
    def snake_name(self) -> str:
        return camelName_to_snake_name(self.name)

    @property
    def typedef_name(self) -> str:
        return f"{self.name}_t"


StringType = Type(
    "string",
    "char*",
    alt_public_type="const char*",
    copy_code=["*dst = bson_strdup(*src);"],
    destroy_code=["bson_free(*value);", "*value = NULL;"],
)
BoolType = Type("boolean", "bool")
BsonStringMappingType = Type(
    "string_mapping",
    "bson_t*",
    alt_public_type="const bson_t*",
    destroy_code=["bson_destroy(*value);", "*value = NULL;"],
)

PARAMETERS = [
    Parameter("appname", StringType),
    Parameter(
        "canonicalizeHostName",
        BoolType,
        deprecation_message="canonicalizeHostName is deprecated. Use authMechanismProperties with CANONICALIZE_HOST_NAME instead.",
    ),
    Parameter(
        "authMechanismProperties",
        BsonStringMappingType,
    ),
]

GENERATED_WARNING = r"""// This code is GENERATED! Do note edit!"""

HEADER_HEADER = f"""{GENERATED_WARNING}
// clang-format off
"""


def _priv_impl_lines(params: Iterable[Parameter]) -> Iterable[str]:
    yield from [
        GENERATED_WARNING,
        "// clang-format off",
        "",
        "#include <mongoc/mongoc-uri.h>",
        "",
        "#include <stdint.h>",
        "",
    ]
    params = tuple(params)
    yield "// typedefs for each parameter's type in storage"
    for p in params:
        yield f"typedef {p.type.var_declspec(p.typedef_name)};"

    yield ""
    yield "typedef struct {"
    for p in params:
        yield f"  // Field for URI parameter '{p.name}'"
        yield "  struct {"
        yield "    bool is_set;"
        yield f"    {p.typedef_name} value;"
        yield f"  }} {p.name};"
    yield "} uri_parameters;"

    types = sorted({p.type.name: p.type for p in params}.values(), key=lambda t: t.name)
    for t in types:
        yield ""
        yield f"static inline void {t.copy_fn_name}({t.spelling}* dst, {t.spelling} const* src) {{"
        yield from (f"  {ln}" for ln in t.get_copy_code())
        yield "}"
        yield ""
        yield f"static inline void {t.destroy_fn_name}({t.spelling}* value) {{"
        yield "  (void)value;"
        yield from (f"  {ln}" for ln in t.destroy_code)
        yield "}"

    yield ""
    yield "static inline void _uri_params_copy(uri_parameters* dst, uri_parameters const* in) {"
    for p in params:
        yield f"  if (in->{p.name}.is_set) {{"
        yield f"    {p.type.copy_fn_name}(&dst->{p.name}.value, &in->{p.name}.value);"
        yield "  }"
    yield "}"

    yield ""
    yield "static inline void _uri_params_destroy(uri_parameters* params) {"
    for p in params:
        yield f"  if (params->{p.name}.is_set) {{"
        yield f"    {p.type.destroy_fn_name}(&params->{p.name}.value);"
        yield f"    params->{p.name}.is_set = false;"
        yield "  }"
    yield "}"


def gen_file(into: IO[bytes], lines: Iterable[str]) -> None:
    for ln in lines:
        into.write(ln.encode())
        into.write(b"\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--header-out", help="File in which to write the generated public API header", required=True)
    parser.add_argument("--c-out", help="File in which to write the generated parameter handling code", required=True)
    args = parser.parse_args()
    with open(args.c_out, "wb") as out:
        gen_file(out, _priv_impl_lines(PARAMETERS))
