#include "test_main.hpp"
#include <tracker/s3m/s3m_types.hpp>

TEST_CASE(S3mTypes_ParapointerHelpers) {
    REQUIRE_EQ(tracker::s3m::parapointer_to_offset(0x0010), 256);
    REQUIRE_EQ(tracker::s3m::offset_to_parapointer(256), 0x0010);
    // Alignment to paragraph boundary (16 bytes)
    REQUIRE_EQ(tracker::s3m::align_paragraph_offset(250), 256);
    REQUIRE_EQ(tracker::s3m::align_paragraph_offset(256), 256);
}

TEST_CASE(S3mTypes_NoteConversion) {
    // Note 1 = C-0 (Octave 0, Semitone 0 -> 0x00)
    REQUIRE_EQ(tracker::s3m::note_to_s3m_byte(1), 0x00);
    REQUIRE_EQ(tracker::s3m::s3m_byte_to_note(0x00), 1);

    // Note 13 = C-1 (Octave 1, Semitone 0 -> 0x10)
    REQUIRE_EQ(tracker::s3m::note_to_s3m_byte(13), 0x10);
    REQUIRE_EQ(tracker::s3m::s3m_byte_to_note(0x10), 13);

    // Note 49 = C-4 (Octave 4, Semitone 0 -> 0x40)
    REQUIRE_EQ(tracker::s3m::note_to_s3m_byte(49), 0x40);
    REQUIRE_EQ(tracker::s3m::s3m_byte_to_note(0x40), 49);

    // Key-off (Note 97 / 0x61) -> 0xFE
    REQUIRE_EQ(tracker::s3m::note_to_s3m_byte(97), 0xFE);
    REQUIRE_EQ(tracker::s3m::s3m_byte_to_note(0xFE), 97);

    // Empty note -> 0xFF
    REQUIRE_EQ(tracker::s3m::note_to_s3m_byte(0), 0xFF);
    REQUIRE_EQ(tracker::s3m::s3m_byte_to_note(0xFF), 0);
}
