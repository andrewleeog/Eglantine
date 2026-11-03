TEST_CASE("Ampere callees can compare into predicates with an R0 link", "[video_core][ampere]") {
    // ISETP's zero destination field is not a write to R0.
    const std::array<Instruction, 5> code{{
        {{0x00007802, 0x20, 0x00000f00, 0}},
        {{0x00007944, 0x10, 0x03c00000, 0}},
        {{0x0000794d, 0, 0x03800000, 0}},
        {{0x0100720c, 0x00000002, 0x03f06070, 0}},
        {{0x00007950, 0xffffffb0, 0x03c3ffff, 0}},
    }};
    REQUIRE(static_cast<bool>(LowerToMaxwell(code)));
}

TEST_CASE("Ampere callees reject a wide multiply over the link", "[video_core][ampere]") {
    // IMAD.WIDE.U32 R54:R55 writes the R55 link.
    const std::array<Instruction, 5> code{{
        {{0x00377802, 0x20, 0x00000f00, 0}},
        {{0x00007944, 0x10, 0x03c00000, 0}},
        {{0x0000794d, 0, 0x03800000, 0}},
        {{0x01367225, 0x00000002, 0x078e00ff, 0}},
        {{0x37007950, 0xffffffb0, 0x03c3ffff, 0}},
    }};
    const auto lowered = LowerToMaxwell(code);
    REQUIRE(!static_cast<bool>(lowered));
    REQUIRE(lowered.error == "native callee overwrites link register");
    REQUIRE(lowered.error_offset == 0x30);
