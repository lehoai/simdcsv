//
// Created by lehoai on 2/8/26.
//
#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include "csv_reader.h"

namespace fs = std::filesystem;

class CsvReaderTest : public ::testing::Test {
protected:
    std::filesystem::path test_dir;

    void SetUp() override {
        test_dir = fs::temp_directory_path() / "csv_test";
        fs::create_directories(test_dir);
    }

    void TearDown() override {
        fs::remove_all(test_dir);
    }

    [[nodiscard]] std::string createTestFile(const std::string& content) const {
        auto path = test_dir / "test.csv";
        std::ofstream file(path, std::ios::binary);
        file << content;
        file.close();
        return path.string();
    }
};

// Test basic parsing
TEST_F(CsvReaderTest, BasicParsing) {
    std::string path = createTestFile("a,b,c\n1,2,3\n4,5,6\n");

    std::vector<std::vector<std::string>> rows;
    constexpr csv::format format;

    csv::CsvReader reader(path.c_str(), format);
    reader.parse([&](const std::string_view* row) {
        rows.push_back({std::string(row[0]), std::string(row[1]), std::string(row[2])});
    });

    ASSERT_EQ(rows.size(), 2);
    EXPECT_EQ(rows[0][0], "1");
    EXPECT_EQ(rows[0][1], "2");
    EXPECT_EQ(rows[0][2], "3");
    EXPECT_EQ(rows[1][0], "4");
    EXPECT_EQ(rows[1][1], "5");
    EXPECT_EQ(rows[1][2], "6");
}

// Test quoted fields
TEST_F(CsvReaderTest, QuotedFields) {
    const std::string path = createTestFile("name,value\n\"hello,world\",123\n");

    std::vector<std::pair<std::string, std::string>> rows;
    csv::format format;
    format.quote = '"';  // Enable quotation

    csv::CsvReader reader(path.c_str(), format);
    reader.parse([&](const std::string_view* row) {
        rows.emplace_back(std::string(row[0]), std::string(row[1]));
    });

    ASSERT_EQ(rows.size(), 1);
    EXPECT_EQ(rows[0].first, "hello,world");
    EXPECT_EQ(rows[0].second, "123");
}

// Test empty fields
TEST_F(CsvReaderTest, EmptyFields) {
    std::string path = createTestFile("a,b,c\n1,,3\n");

    std::vector<std::string> row0;
    constexpr csv::format format;

    csv::CsvReader reader(path.c_str(), format);
    reader.parse([&](const std::string_view* row) {
        row0 = {std::string(row[0]), std::string(row[1]), std::string(row[2])};
    });

    EXPECT_EQ(row0[0], "1");
    EXPECT_EQ(row0[1], "");
    EXPECT_EQ(row0[2], "3");
}

// Test header row selection
TEST_F(CsvReaderTest, HeaderRowSelection) {
    std::string path = createTestFile("skip\na,b\n1,2\n");

    csv::format format;
    format.header_row = 1;  // Use second row as header

    int row_count = 0;
    csv::CsvReader reader(path.c_str(), format);
    reader.parse([&](const std::string_view* row) {
        row_count++;
    });

    EXPECT_EQ(row_count, 1);  // Only "1,2" should be parsed as data
}

// Test tab delimiter
TEST_F(CsvReaderTest, TabDelimiter) {
    std::string path = createTestFile("a\tb\tc\n1\t2\t3\n4\t5\t6\n");

    csv::format format;
    format.delimiter = '\t';

    std::vector<std::vector<std::string>> rows;
    csv::CsvReader reader(path.c_str(), format);
    reader.parse([&](const std::string_view* row) {
        rows.push_back({std::string(row[0]), std::string(row[1]), std::string(row[2])});
    });

    ASSERT_EQ(rows.size(), 2);
    EXPECT_EQ(rows[0][0], "1");
    EXPECT_EQ(rows[0][1], "2");
    EXPECT_EQ(rows[0][2], "3");
    EXPECT_EQ(rows[1][0], "4");
    EXPECT_EQ(rows[1][1], "5");
    EXPECT_EQ(rows[1][2], "6");
}

// Test custom delimiter
TEST_F(CsvReaderTest, CustomDelimiter) {
    std::string path = createTestFile("a;b;c\n1;2;3\n");

    csv::format format;
    format.delimiter = ';';

    std::vector<std::string> row0;
    csv::CsvReader reader(path.c_str(), format);
    reader.parse([&](const std::string_view* row) {
        row0 = {std::string(row[0]), std::string(row[1]), std::string(row[2])};
    });

    EXPECT_EQ(row0[0], "1");
    EXPECT_EQ(row0[1], "2");
    EXPECT_EQ(row0[2], "3");
}

// Test trim_quotes function
TEST(TrimQuotesTest, BasicTrim) {
    csv::format format;
    format.quote = '"';  // Enable quotation

    EXPECT_EQ(csv::trim_quotes("\"hello\"", format), "hello");
    EXPECT_EQ(csv::trim_quotes("hello", format), "hello");
    EXPECT_EQ(csv::trim_quotes("\"\"", format), "");
    EXPECT_EQ(csv::trim_quotes("\"", format), "\"");  // Single quote unchanged
}

// Test trim_quotes without quotation
TEST(TrimQuotesTest, NoQuotation) {
    csv::format format;
    // format.quote is std::nullopt by default

    EXPECT_EQ(csv::trim_quotes("\"hello\"", format), "\"hello\"");
    EXPECT_EQ(csv::trim_quotes("hello", format), "hello");
    EXPECT_EQ(csv::trim_quotes("\"\"", format), "\"\"");
}

// prefix XOR
TEST(PrefixXorTest, Correctness) {

    EXPECT_EQ(csv::prefix_xor(0b00000000), 0b00000000);

    EXPECT_EQ(csv::prefix_xor(0b00000001), 0xFFFFFFFF);

    EXPECT_EQ(csv::prefix_xor(0b00100100), 0b00011100);

    EXPECT_EQ(csv::prefix_xor(0b10000000) & 0xFF, 0b10000000);
}

// // Test getInt helper
TEST(GetIntTest, BasicConversion) {
    EXPECT_EQ(csv::get<int>("123"), 123);
    EXPECT_EQ(csv::get<int>("-456"), -456);
    EXPECT_EQ(csv::get<int>("0"), 0);

    EXPECT_DOUBLE_EQ(csv::get<double>("123.456"), 123.456);
    EXPECT_DOUBLE_EQ(csv::get<double>("-78.9"), -78.9);
    EXPECT_DOUBLE_EQ(csv::get<double>("0.0"), 0.0);
    EXPECT_DOUBLE_EQ(csv::get<double>("3.14159"), 3.14159);
    EXPECT_DOUBLE_EQ(csv::get<double>("1e10"), 1e10);
    EXPECT_DOUBLE_EQ(csv::get<double>("-2.5e-3"), -2.5e-3);
}

// Test no trailing newline
TEST_F(CsvReaderTest, NoTrailingNewline) {
    std::string path = createTestFile("a,b,c\n1,2,3\n4,5,6");  // No \n at end

    std::vector<std::vector<std::string>> rows;
    constexpr csv::format format;

    csv::CsvReader reader(path.c_str(), format);
    reader.parse([&](const std::string_view* row) {
        rows.push_back({std::string(row[0]), std::string(row[1]), std::string(row[2])});
    });

    ASSERT_EQ(rows.size(), 2);
    EXPECT_EQ(rows[0][0], "1");
    EXPECT_EQ(rows[0][1], "2");
    EXPECT_EQ(rows[0][2], "3");
    EXPECT_EQ(rows[1][0], "4");
    EXPECT_EQ(rows[1][1], "5");
    EXPECT_EQ(rows[1][2], "6");
}

// Test quoted field with newline inside
TEST_F(CsvReaderTest, QuotedFieldWithNewline) {
    std::string path = createTestFile("name,desc\n\"John\",\"Line1\nLine2\"\n");

    std::vector<std::pair<std::string, std::string>> rows;
    csv::format format;
    format.quote = '"';  // Enable quotation

    csv::CsvReader reader(path.c_str(), format);
    reader.parse([&](const std::string_view* row) {
        rows.emplace_back(std::string(row[0]), std::string(row[1]));
    });

    ASSERT_EQ(rows.size(), 1);
    EXPECT_EQ(rows[0].first, "John");
    EXPECT_EQ(rows[0].second, "Line1\nLine2");
}

// Test multiple consecutive empty fields
TEST_F(CsvReaderTest, MultipleEmptyFields) {
    std::string path = createTestFile("a,b,c,d,e\n1,,,4,5\n");

    std::vector<std::string> row0;
    constexpr csv::format format;

    csv::CsvReader reader(path.c_str(), format);
    reader.parse([&](const std::string_view* row) {
        row0 = {std::string(row[0]), std::string(row[1]), std::string(row[2]), 
                std::string(row[3]), std::string(row[4])};
    });

    EXPECT_EQ(row0[0], "1");
    EXPECT_EQ(row0[1], "");
    EXPECT_EQ(row0[2], "");
    EXPECT_EQ(row0[3], "4");
    EXPECT_EQ(row0[4], "5");
}

// Test getHeaders function
TEST_F(CsvReaderTest, GetHeaders) {
    std::string path = createTestFile("name,age,city\nAlice,25,NYC\n");

    csv::format format;
    csv::CsvReader reader(path.c_str(), format);
    
    // Parse first to populate headers
    reader.parse([&](const std::string_view* row) {});

    auto headers = reader.getHeaders();
    ASSERT_EQ(headers.size(), 3);
    EXPECT_EQ(headers[0], "name");
    EXPECT_EQ(headers[1], "age");
    EXPECT_EQ(headers[2], "city");
}

// Test long line (exceeds 32 bytes to test SIMD loop)
TEST_F(CsvReaderTest, LongLine) {
    std::string path = createTestFile(
        "col1,col2,col3,col4,col5,col6,col7,col8,col9,col10\n"
        "verylongvalue1,verylongvalue2,verylongvalue3,verylongvalue4,verylongvalue5,"
        "verylongvalue6,verylongvalue7,verylongvalue8,verylongvalue9,verylongvalue10\n"
    );

    int row_count = 0;
    constexpr csv::format format;

    csv::CsvReader reader(path.c_str(), format);
    auto col_num = reader.getHeaders().size();
    reader.parse([&](const std::string_view* row) {
        EXPECT_EQ(col_num, 10);
        if (row_count == 0) {
            EXPECT_EQ(std::string(row[0]), "verylongvalue1");
            EXPECT_EQ(std::string(row[9]), "verylongvalue10");
        }
        row_count++;
    });

    EXPECT_EQ(row_count, 1);
}

// Test single column
TEST_F(CsvReaderTest, SingleColumn) {
    std::string path = createTestFile("value\n1\n2\n3\n");

    std::vector<std::string> values;
    constexpr csv::format format;

    csv::CsvReader reader(path.c_str(), format);
    auto col_num = reader.getHeaders().size();
    reader.parse([&](const std::string_view* row) {
        EXPECT_EQ(col_num, 1);
        values.emplace_back(row[0]);
    });

    ASSERT_EQ(values.size(), 3);
    EXPECT_EQ(values[0], "1");
    EXPECT_EQ(values[1], "2");
    EXPECT_EQ(values[2], "3");
}

// Test only header (no data rows)
TEST_F(CsvReaderTest, OnlyHeader) {
    std::string path = createTestFile("a,b,c\n");

    int row_count = 0;
    constexpr csv::format format;

    csv::CsvReader reader(path.c_str(), format);
    reader.parse([&](const std::string_view* row) {
        row_count++;
    });

    EXPECT_EQ(row_count, 0);
    
    auto headers = reader.getHeaders();
    ASSERT_EQ(headers.size(), 3);
    EXPECT_EQ(headers[0], "a");
    EXPECT_EQ(headers[1], "b");
    EXPECT_EQ(headers[2], "c");
}

// Test custom quote character
TEST_F(CsvReaderTest, CustomQuoteCharacter) {
    std::string path = createTestFile("name,value\n'hello,world',123\n");

    csv::format format;
    format.quote = '\'';

    std::vector<std::pair<std::string, std::string>> rows;
    csv::CsvReader reader(path.c_str(), format);
    reader.parse([&](const std::string_view* row) {
        rows.emplace_back(std::string(row[0]), std::string(row[1]));
    });

    ASSERT_EQ(rows.size(), 1);
    EXPECT_EQ(rows[0].first, "hello,world");
    EXPECT_EQ(rows[0].second, "123");
}

// Test trailing commas
TEST_F(CsvReaderTest, TrailingComma) {
    std::string path = createTestFile("a,b,c,\n1,2,3,\n");

    std::vector<std::string> row0;
    constexpr csv::format format;

    csv::CsvReader reader(path.c_str(), format);
    auto col_num = reader.getHeaders().size();
    reader.parse([&](const std::string_view* row) {
        EXPECT_EQ(col_num, 4);
        row0 = {std::string(row[0]), std::string(row[1]), 
                std::string(row[2]), std::string(row[3])};
    });

    EXPECT_EQ(row0[0], "1");
    EXPECT_EQ(row0[1], "2");
    EXPECT_EQ(row0[2], "3");
    EXPECT_EQ(row0[3], "");  // Trailing comma creates empty field
}

// Test empty quoted field
TEST_F(CsvReaderTest, EmptyQuotedField) {
    std::string path = createTestFile("a,b,c\n1,\"\",3\n");

    std::vector<std::string> row0;
    csv::format format;
    format.quote = '"';  // Enable quotation

    csv::CsvReader reader(path.c_str(), format);
    reader.parse([&](const std::string_view* row) {
        row0 = {std::string(row[0]), std::string(row[1]), std::string(row[2])};
    });

    EXPECT_EQ(row0[0], "1");
    EXPECT_EQ(row0[1], "");
    EXPECT_EQ(row0[2], "3");
}

// Test whitespace in fields
TEST_F(CsvReaderTest, WhitespaceInFields) {
    std::string path = createTestFile("a,b,c\n  spaces  ,\ttabs\t,normal\n");

    std::vector<std::string> row0;
    constexpr csv::format format;

    csv::CsvReader reader(path.c_str(), format);
    reader.parse([&](const std::string_view* row) {
        row0 = {std::string(row[0]), std::string(row[1]), std::string(row[2])};
    });

    EXPECT_EQ(row0[0], "  spaces  ");
    EXPECT_EQ(row0[1], "\ttabs\t");
    EXPECT_EQ(row0[2], "normal");
}

// Test multiple header rows with different header_row selection
TEST_F(CsvReaderTest, MultipleHeaderRows) {
    std::string path = createTestFile("metadata\nreal_header_a,real_header_b\n1,2\n3,4\n");

    csv::format format;
    format.header_row = 1;  // Second row is header

    std::vector<std::vector<std::string>> rows;
    csv::CsvReader reader(path.c_str(), format);
    reader.parse([&](const std::string_view* row) {
        rows.push_back({std::string(row[0]), std::string(row[1])});
    });

    ASSERT_EQ(rows.size(), 2);
    EXPECT_EQ(rows[0][0], "1");
    EXPECT_EQ(rows[0][1], "2");
    EXPECT_EQ(rows[1][0], "3");
    EXPECT_EQ(rows[1][1], "4");
    
    auto headers = reader.getHeaders();
    ASSERT_EQ(headers.size(), 2);
    EXPECT_EQ(headers[0], "real_header_a");
    EXPECT_EQ(headers[1], "real_header_b");
}

// ==================== NO QUOTATION TEST CASES ====================

// Test no quotation - basic parsing
TEST_F(CsvReaderTest, NoQuotation_BasicParsing) {
    std::string path = createTestFile("a,b,c\n1,2,3\n4,5,6\n");

    std::vector<std::vector<std::string>> rows;
    csv::format format;
    // format.quote is std::nullopt by default

    csv::CsvReader reader(path.c_str(), format);
    reader.parse([&](const std::string_view* row) {
        rows.push_back({std::string(row[0]), std::string(row[1]), std::string(row[2])});
    });

    ASSERT_EQ(rows.size(), 2);
    EXPECT_EQ(rows[0][0], "1");
    EXPECT_EQ(rows[0][1], "2");
    EXPECT_EQ(rows[0][2], "3");
    EXPECT_EQ(rows[1][0], "4");
    EXPECT_EQ(rows[1][1], "5");
    EXPECT_EQ(rows[1][2], "6");
}

// Test no quotation - quotes are treated as regular characters
TEST_F(CsvReaderTest, NoQuotation_QuotesAsLiterals) {
    std::string path = createTestFile("a,b,c\n\"hello\",\"world\",\"test\"\n");

    std::vector<std::string> row0;
    csv::format format;
    // format.quote is std::nullopt by default

    csv::CsvReader reader(path.c_str(), format);
    reader.parse([&](const std::string_view* row) {
        row0 = {std::string(row[0]), std::string(row[1]), std::string(row[2])};
    });

    // Quotes should be preserved as literal characters
    EXPECT_EQ(row0[0], "\"hello\"");
    EXPECT_EQ(row0[1], "\"world\"");
    EXPECT_EQ(row0[2], "\"test\"");
}

// Test no quotation - comma in field breaks it
TEST_F(CsvReaderTest, NoQuotation_CommaBreaksField) {
    std::string path = createTestFile("a,b,c,d\nhello,world,foo,bar\n");

    int col_count = 0;
    std::vector<std::string> row0;
    csv::format format;

    csv::CsvReader reader(path.c_str(), format);
    auto col_num = reader.getHeaders().size();
    reader.parse([&](const std::string_view* row) {
        col_count = col_num;
        for (int i = 0; i < col_num; i++) {
            row0.emplace_back(row[i]);
        }
    });


    EXPECT_EQ(col_count, 4);
    ASSERT_EQ(row0.size(), 4);
    EXPECT_EQ(row0[0], "hello");
    EXPECT_EQ(row0[1], "world");
    EXPECT_EQ(row0[2], "foo");
    EXPECT_EQ(row0[3], "bar");
}

// Test no quotation - long line
TEST_F(CsvReaderTest, NoQuotation_LongLine) {
    std::string path = createTestFile(
        "col1,col2,col3,col4,col5\n"
        "verylongvalue1,verylongvalue2,verylongvalue3,verylongvalue4,verylongvalue5\n"
    );

    int row_count = 0;
    csv::format format;

    csv::CsvReader reader(path.c_str(), format);
    auto col_num = reader.getHeaders().size();
    reader.parse([&](const std::string_view* row) {
        EXPECT_EQ(col_num, 5);
        if (row_count == 0) {
            EXPECT_EQ(std::string(row[0]), "verylongvalue1");
            EXPECT_EQ(std::string(row[4]), "verylongvalue5");
        }
        row_count++;
    });

    EXPECT_EQ(row_count, 1);
}

// Test no quotation - tab delimiter
TEST_F(CsvReaderTest, NoQuotation_TabDelimiter) {
    std::string path = createTestFile("a\tb\tc\n1\t2\t3\n");

    csv::format format;
    format.delimiter = '\t';

    std::vector<std::string> row0;
    csv::CsvReader reader(path.c_str(), format);
    reader.parse([&](const std::string_view* row) {
        row0 = {std::string(row[0]), std::string(row[1]), std::string(row[2])};
    });

    EXPECT_EQ(row0[0], "1");
    EXPECT_EQ(row0[1], "2");
    EXPECT_EQ(row0[2], "3");
}

// Test no quotation - empty fields
TEST_F(CsvReaderTest, NoQuotation_EmptyFields) {
    std::string path = createTestFile("a,b,c\n1,,3\n,2,\n");

    csv::format format;

    std::vector<std::vector<std::string>> rows;
    csv::CsvReader reader(path.c_str(), format);
    reader.parse([&](const std::string_view* row) {
        rows.push_back({std::string(row[0]), std::string(row[1]), std::string(row[2])});
    });

    ASSERT_EQ(rows.size(), 2);
    EXPECT_EQ(rows[0][0], "1");
    EXPECT_EQ(rows[0][1], "");
    EXPECT_EQ(rows[0][2], "3");
    EXPECT_EQ(rows[1][0], "");
    EXPECT_EQ(rows[1][1], "2");
    EXPECT_EQ(rows[1][2], "");
}

// Test no quotation - mixed with quote characters
TEST_F(CsvReaderTest, NoQuotation_MixedQuoteCharacters) {
    std::string path = createTestFile("a,b,c\n\"partial,comma\",normal,\"both\"ends\"\n");

    csv::format format;

    std::vector<std::string> row0;
    csv::CsvReader reader(path.c_str(), format);
    auto col_num = reader.getHeaders().size();
    reader.parse([&](const std::string_view* row) {
        for (int i = 0; i < col_num; i++) {
            row0.emplace_back(row[i]);
        }
    });

    // Without quote processing, comma inside quotes breaks the field
    ASSERT_GE(row0.size(), 3);
    EXPECT_EQ(row0[0], "\"partial");
    EXPECT_EQ(row0[1], "comma\"");
    EXPECT_EQ(row0[2], "normal");
}

// Test custom newline - semicolon as newline
TEST_F(CsvReaderTest, CustomNewline_Semicolon) {
    std::string path = createTestFile("a,b,c;1,2,3;4,5,6;");

    csv::format format;
    format.new_line = ';';

    std::vector<std::vector<std::string>> rows;
    csv::CsvReader reader(path.c_str(), format);
    reader.parse([&](const std::string_view* row) {
        rows.push_back({std::string(row[0]), std::string(row[1]), std::string(row[2])});
    });

    ASSERT_EQ(rows.size(), 2);
    EXPECT_EQ(rows[0][0], "1");
    EXPECT_EQ(rows[0][1], "2");
    EXPECT_EQ(rows[0][2], "3");
    EXPECT_EQ(rows[1][0], "4");
    EXPECT_EQ(rows[1][1], "5");
    EXPECT_EQ(rows[1][2], "6");
}

// Test custom newline - pipe as newline
TEST_F(CsvReaderTest, CustomNewline_Pipe) {
    std::string path = createTestFile("name,age|Alice,25|Bob,30|");

    csv::format format;
    format.new_line = '|';

    std::vector<std::vector<std::string>> rows;
    csv::CsvReader reader(path.c_str(), format);
    reader.parse([&](const std::string_view* row) {
        rows.push_back({std::string(row[0]), std::string(row[1])});
    });

    ASSERT_EQ(rows.size(), 2);
    EXPECT_EQ(rows[0][0], "Alice");
    EXPECT_EQ(rows[0][1], "25");
    EXPECT_EQ(rows[1][0], "Bob");
    EXPECT_EQ(rows[1][1], "30");
}

// Test custom newline - carriage return
TEST_F(CsvReaderTest, CustomNewline_CarriageReturn) {
    std::string path = createTestFile("a,b,c\r1,2,3\r4,5,6\r");

    csv::format format;
    format.new_line = '\r';

    std::vector<std::vector<std::string>> rows;
    csv::CsvReader reader(path.c_str(), format);
    reader.parse([&](const std::string_view* row) {
        rows.push_back({std::string(row[0]), std::string(row[1]), std::string(row[2])});
    });

    ASSERT_EQ(rows.size(), 2);
    EXPECT_EQ(rows[0][0], "1");
    EXPECT_EQ(rows[0][1], "2");
    EXPECT_EQ(rows[0][2], "3");
    EXPECT_EQ(rows[1][0], "4");
    EXPECT_EQ(rows[1][1], "5");
    EXPECT_EQ(rows[1][2], "6");
}

// Test custom newline with custom delimiter
TEST_F(CsvReaderTest, CustomNewline_WithCustomDelimiter) {
    std::string path = createTestFile("a;b;c|1;2;3|4;5;6|");

    csv::format format;
    format.delimiter = ';';
    format.new_line = '|';

    std::vector<std::vector<std::string>> rows;
    csv::CsvReader reader(path.c_str(), format);
    reader.parse([&](const std::string_view* row) {
        rows.push_back({std::string(row[0]), std::string(row[1]), std::string(row[2])});
    });

    ASSERT_EQ(rows.size(), 2);
    EXPECT_EQ(rows[0][0], "1");
    EXPECT_EQ(rows[0][1], "2");
    EXPECT_EQ(rows[0][2], "3");
    EXPECT_EQ(rows[1][0], "4");
    EXPECT_EQ(rows[1][1], "5");
    EXPECT_EQ(rows[1][2], "6");
}

// Test custom newline with quotation
TEST_F(CsvReaderTest, CustomNewline_WithQuotation) {
    std::string path = createTestFile("name,value;\"hello;world\",123;\"test\",456;");

    csv::format format;
    format.new_line = ';';
    format.quote = '"';

    std::vector<std::pair<std::string, std::string>> rows;
    csv::CsvReader reader(path.c_str(), format);
    reader.parse([&](const std::string_view* row) {
        rows.emplace_back(std::string(row[0]), std::string(row[1]));
    });

    ASSERT_EQ(rows.size(), 2);
    EXPECT_EQ(rows[0].first, "hello;world");
    EXPECT_EQ(rows[0].second, "123");
    EXPECT_EQ(rows[1].first, "test");
    EXPECT_EQ(rows[1].second, "456");
}

// Test custom newline - no trailing newline
TEST_F(CsvReaderTest, CustomNewline_NoTrailing) {
    std::string path = createTestFile("a,b,c;1,2,3;4,5,6");  // No trailing ;

    csv::format format;
    format.new_line = ';';

    std::vector<std::vector<std::string>> rows;
    csv::CsvReader reader(path.c_str(), format);
    reader.parse([&](const std::string_view* row) {
        rows.push_back({std::string(row[0]), std::string(row[1]), std::string(row[2])});
    });

    ASSERT_EQ(rows.size(), 2);
    EXPECT_EQ(rows[0][0], "1");
    EXPECT_EQ(rows[0][1], "2");
    EXPECT_EQ(rows[0][2], "3");
    EXPECT_EQ(rows[1][0], "4");
    EXPECT_EQ(rows[1][1], "5");
    EXPECT_EQ(rows[1][2], "6");
}

// Test custom newline - long line (SIMD)
TEST_F(CsvReaderTest, CustomNewline_LongLine) {
    std::string path = createTestFile(
        "col1,col2,col3,col4,col5,col6,col7,col8,col9,col10;"
        "verylongvalue1,verylongvalue2,verylongvalue3,verylongvalue4,verylongvalue5,"
        "verylongvalue6,verylongvalue7,verylongvalue8,verylongvalue9,verylongvalue10"
    );

    csv::format format;
    format.new_line = ';';

    int row_count = 0;
    csv::CsvReader reader(path.c_str(), format);
    auto col_num = reader.getHeaders().size();
    reader.parse([&](const std::string_view* row) {
        EXPECT_EQ(col_num, 10);
        if (row_count == 0) {
            EXPECT_EQ(std::string(row[0]), "verylongvalue1");
            EXPECT_EQ(std::string(row[9]), "verylongvalue10");
        }
        row_count++;
    });

    EXPECT_EQ(row_count, 1);
}

// Test custom newline - single column
TEST_F(CsvReaderTest, CustomNewline_SingleColumn) {
    std::string path = createTestFile("value;1;2;3;");

    csv::format format;
    format.new_line = ';';

    std::vector<std::string> values;
    csv::CsvReader reader(path.c_str(), format);
    auto col_num = reader.getHeaders().size();
    reader.parse([&](const std::string_view* row) {
        EXPECT_EQ(col_num, 1);
        values.emplace_back(row[0]);
    });

    ASSERT_EQ(values.size(), 3);
    EXPECT_EQ(values[0], "1");
    EXPECT_EQ(values[1], "2");
    EXPECT_EQ(values[2], "3");
}

// Test custom newline - empty fields
TEST_F(CsvReaderTest, CustomNewline_EmptyFields) {
    std::string path = createTestFile("a,b,c;1,,3;,2,;");

    csv::format format;
    format.new_line = ';';

    std::vector<std::vector<std::string>> rows;
    csv::CsvReader reader(path.c_str(), format);
    reader.parse([&](const std::string_view* row) {
        rows.push_back({std::string(row[0]), std::string(row[1]), std::string(row[2])});
    });

    ASSERT_EQ(rows.size(), 2);
    EXPECT_EQ(rows[0][0], "1");
    EXPECT_EQ(rows[0][1], "");
    EXPECT_EQ(rows[0][2], "3");
    EXPECT_EQ(rows[1][0], "");
    EXPECT_EQ(rows[1][1], "2");
    EXPECT_EQ(rows[1][2], "");
}

// Test custom newline - header row selection
TEST_F(CsvReaderTest, CustomNewline_HeaderRowSelection) {
    std::string path = createTestFile("skip;real_header_a,real_header_b;1,2;");

    csv::format format;
    format.new_line = ';';
    format.header_row = 1;  // Second row is header

    int row_count = 0;
    csv::CsvReader reader(path.c_str(), format);
    reader.parse([&](const std::string_view* row) {
        row_count++;
    });

    EXPECT_EQ(row_count, 1);
    
    auto headers = reader.getHeaders();
    ASSERT_EQ(headers.size(), 2);
    EXPECT_EQ(headers[0], "real_header_a");
    EXPECT_EQ(headers[1], "real_header_b");
}

// Test custom newline - tab delimiter + pipe newline
TEST_F(CsvReaderTest, CustomNewline_TabAndPipe) {
    std::string path = createTestFile("a\tb\tc|1\t2\t3|4\t5\t6|");

    csv::format format;
    format.delimiter = '\t';
    format.new_line = '|';

    std::vector<std::vector<std::string>> rows;
    csv::CsvReader reader(path.c_str(), format);
    reader.parse([&](const std::string_view* row) {
        rows.push_back({std::string(row[0]), std::string(row[1]), std::string(row[2])});
    });

    ASSERT_EQ(rows.size(), 2);
    EXPECT_EQ(rows[0][0], "1");
    EXPECT_EQ(rows[0][1], "2");
    EXPECT_EQ(rows[0][2], "3");
    EXPECT_EQ(rows[1][0], "4");
    EXPECT_EQ(rows[1][1], "5");
    EXPECT_EQ(rows[1][2], "6");
}

// ==================== COLUMN COUNT MISMATCH TEST CASES ====================

// Test data row with fewer columns than header
TEST_F(CsvReaderTest, FewerColumnsThanHeader) {
    std::string path = createTestFile("a,b,c,d\n1,2,3\n4,5\n");

    csv::format format;

    std::vector<std::vector<std::string>> rows;
    csv::CsvReader reader(path.c_str(), format);
    auto col_num = reader.getHeaders().size();
    reader.parse([&](const std::string_view* row) {
        EXPECT_EQ(col_num, 4);  // Header defines 4 columns
        std::vector<std::string> r;
        for (int i = 0; i < col_num; i++) {
            r.emplace_back(row[i]);
        }
        rows.push_back(r);
    });

    ASSERT_EQ(rows.size(), 2);
    
    // First row: only 3 values provided
    EXPECT_EQ(rows[0][0], "1");
    EXPECT_EQ(rows[0][1], "2");
    EXPECT_EQ(rows[0][2], "3");
    // rows[0][3] may contain garbage - this documents current behavior
    
    // Second row: only 2 values provided
    EXPECT_EQ(rows[1][0], "4");
    EXPECT_EQ(rows[1][1], "5");
}

// Test data row with more columns than header
TEST_F(CsvReaderTest, MoreColumnsThanHeader) {
    std::string path = createTestFile("a,b,c\n1,2,3,4,5\n6,7,8,9\n");

    csv::format format;

    std::vector<std::vector<std::string>> rows;
    csv::CsvReader reader(path.c_str(), format);
    auto col_num = reader.getHeaders().size();
    reader.parse([&](const std::string_view* row) {
        EXPECT_EQ(col_num, 3);  // Header defines 3 columns
        std::vector<std::string> r;
        for (int i = 0; i < col_num; i++) {
            r.emplace_back(row[i]);
        }
        rows.push_back(r);
    });

    ASSERT_EQ(rows.size(), 2);
    
    // Only first 3 columns should be captured, extras dropped
    EXPECT_EQ(rows[0][0], "1");
    EXPECT_EQ(rows[0][1], "2");
    EXPECT_EQ(rows[0][2], "3");
    
    EXPECT_EQ(rows[1][0], "6");
    EXPECT_EQ(rows[1][1], "7");
    EXPECT_EQ(rows[1][2], "8");
}

// Test mixed column counts across rows
TEST_F(CsvReaderTest, MixedColumnCounts) {
    std::string path = createTestFile("a,b,c\n1,2,3\n4,5\n6,7,8,9,10\n");

    csv::format format;

    int row_count = 0;
    csv::CsvReader reader(path.c_str(), format);
    auto col_num = reader.getHeaders().size();
    reader.parse([&](const std::string_view* row) {
        EXPECT_EQ(col_num, 3);  // Header always defines column count
        
        if (row_count == 0) {
            EXPECT_EQ(std::string(row[0]), "1");
            EXPECT_EQ(std::string(row[1]), "2");
            EXPECT_EQ(std::string(row[2]), "3");
        } else if (row_count == 1) {
            EXPECT_EQ(std::string(row[0]), "4");
            EXPECT_EQ(std::string(row[1]), "5");
        } else if (row_count == 2) {
            EXPECT_EQ(std::string(row[0]), "6");
            EXPECT_EQ(std::string(row[1]), "7");
            EXPECT_EQ(std::string(row[2]), "8");
        }
        row_count++;
    });

    EXPECT_EQ(row_count, 3);
}

// Test row with all empty fields
TEST_F(CsvReaderTest, AllEmptyFieldsRow) {
    std::string path = createTestFile("a,b,c\n,,\n1,2,3\n");

    csv::format format;

    std::vector<std::vector<std::string>> rows;
    csv::CsvReader reader(path.c_str(), format);
    auto col_num = reader.getHeaders().size();
    reader.parse([&](const std::string_view* row) {
        std::vector<std::string> r;
        for (int i = 0; i < col_num; i++) {
            r.emplace_back(row[i]);
        }
        rows.push_back(r);
    });

    ASSERT_EQ(rows.size(), 2);
    EXPECT_EQ(rows[0][0], "");
    EXPECT_EQ(rows[0][1], "");
    EXPECT_EQ(rows[0][2], "");
    EXPECT_EQ(rows[1][0], "1");
    EXPECT_EQ(rows[1][1], "2");
    EXPECT_EQ(rows[1][2], "3");
}

// Test many extra fields to verify no buffer overflow
TEST_F(CsvReaderTest, ManyExtraFields) {
    std::string path = createTestFile("a,b\n1,2,3,4,5,6,7,8,9,10,11,12,13,14,15\n");

    csv::format format;

    int row_count = 0;
    csv::CsvReader reader(path.c_str(), format);
    auto col_num = reader.getHeaders().size();
    reader.parse([&](const std::string_view* row) {
        EXPECT_EQ(col_num, 2);
        EXPECT_EQ(std::string(row[0]), "1");
        EXPECT_EQ(std::string(row[1]), "2");
        // All extra fields 3-15 should be safely dropped
        row_count++;
    });

    EXPECT_EQ(row_count, 1);
}