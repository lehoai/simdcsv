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

    csv::CsvReader::parse(path.c_str(), format, [&](const std::string_view* row) {
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
    const csv::format format;

    csv::CsvReader::parse(path.c_str(), format, [&](const std::string_view* row) {
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
    csv::format format;

    csv::CsvReader::parse(path.c_str(), format, [&](const std::string_view* row) {
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
    csv::CsvReader::parse(path.c_str(), format, [&](const std::string_view* row) {
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
    csv::CsvReader::parse(path.c_str(), format, [&](const std::string_view* row) {
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
    csv::CsvReader::parse(path.c_str(), format, [&](const std::string_view* row) {
        row0 = {std::string(row[0]), std::string(row[1]), std::string(row[2])};
    });

    EXPECT_EQ(row0[0], "1");
    EXPECT_EQ(row0[1], "2");
    EXPECT_EQ(row0[2], "3");
}

// Test trim_quotes function
TEST(TrimQuotesTest, BasicTrim) {
    csv::format format;

    EXPECT_EQ(csv::trim_quotes("\"hello\"", format), "hello");
    EXPECT_EQ(csv::trim_quotes("hello", format), "hello");
    EXPECT_EQ(csv::trim_quotes("\"\"", format), "");
    EXPECT_EQ(csv::trim_quotes("\"", format), "\"");  // Single quote unchanged
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