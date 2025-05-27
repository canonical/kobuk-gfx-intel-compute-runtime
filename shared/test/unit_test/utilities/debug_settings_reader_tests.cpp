/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/api_specific_config.h"
#include "shared/test/common/mocks/mock_settings_reader.h"
#include "shared/test/common/test_macros/test.h"

#include "gtest/gtest.h"

#include <fstream>
#include <string>

using namespace NEO;

namespace SettingsReaderTests {

TEST(SettingsReader, WhenCreatingSettingsReaderThenReaderIsCreated) {
    auto reader = std::unique_ptr<SettingsReader>(SettingsReader::create(ApiSpecificConfig::getRegistryPath()));
    EXPECT_NE(nullptr, reader.get());
}

TEST(SettingsReader, GivenNoSettingsFileWhenCreatingSettingsReaderThenOsReaderIsCreated) {
    ASSERT_FALSE(virtualFileExists(SettingsReader::settingsFileName));

    auto fileReader = std::unique_ptr<SettingsReader>(SettingsReader::createFileReader());
    EXPECT_EQ(nullptr, fileReader.get());

    auto osReader = std::unique_ptr<SettingsReader>(SettingsReader::create(ApiSpecificConfig::getRegistryPath()));
    EXPECT_NE(nullptr, osReader.get());
}

TEST(SettingsReader, GivenSettingsFileExistsWhenCreatingSettingsReaderThenReaderIsCreated) {
    ASSERT_FALSE(virtualFileExists(SettingsReader::settingsFileName));

    constexpr std::string_view data = "ProductFamilyOverride = test";
    writeDataToFile(SettingsReader::settingsFileName, data);

    auto reader = std::unique_ptr<SettingsReader>(MockSettingsReader::create(ApiSpecificConfig::getRegistryPath()));
    EXPECT_NE(nullptr, reader.get());
    std::string defaultValue("unk");
    EXPECT_STREQ("test", reader->getSetting("ProductFamilyOverride", defaultValue).c_str());

    removeVirtualFile(SettingsReader::settingsFileName);
}

TEST(SettingsReader, WhenCreatingFileReaderThenReaderIsCreated) {
    ASSERT_FALSE(virtualFileExists(SettingsReader::settingsFileName));
    char data = 0;
    std::string_view emptyView(&data, 0);
    writeDataToFile(SettingsReader::settingsFileName, emptyView);

    auto reader = std::unique_ptr<SettingsReader>(MockSettingsReader::createFileReader());
    EXPECT_NE(nullptr, reader.get());

    removeVirtualFile(SettingsReader::settingsFileName);
}

TEST(SettingsReader, WhenCreatingFileReaderUseNeoFileIfNoDefault) {
    ASSERT_FALSE(virtualFileExists(SettingsReader::settingsFileName));
    ASSERT_FALSE(virtualFileExists(SettingsReader::neoSettingsFileName));
    char data = 0;
    std::string_view emptyView(&data, 0);
    writeDataToFile(SettingsReader::neoSettingsFileName, emptyView);

    auto reader = std::unique_ptr<SettingsReader>(MockSettingsReader::createFileReader());
    EXPECT_NE(nullptr, reader.get());

    removeVirtualFile(SettingsReader::neoSettingsFileName);
}

TEST(SettingsReader, WhenCreatingOsReaderThenReaderIsCreated) {
    auto reader = std::unique_ptr<SettingsReader>(SettingsReader::createOsReader(false, ApiSpecificConfig::getRegistryPath()));
    EXPECT_NE(nullptr, reader.get());
}

TEST(SettingsReader, GivenRegKeyWhenCreatingOsReaderThenReaderIsCreated) {
    std::string regKey = ApiSpecificConfig::getRegistryPath();
    auto reader = std::unique_ptr<SettingsReader>(SettingsReader::createOsReader(false, regKey));
    EXPECT_NE(nullptr, reader.get());
}

TEST(SettingsReader, GivenTrueWhenPrintingDebugStringThenPrintsToOutput) {
    int i = 4;
    testing::internal::CaptureStdout();
    PRINT_DEBUG_STRING(true, stdout, "testing error %d", i);
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_STRNE(output.c_str(), "");
}

TEST(SettingsReader, GivenFalseWhenPrintingDebugStringThenNoOutput) {
    int i = 4;
    testing::internal::CaptureStdout();
    PRINT_DEBUG_STRING(false, stderr, "Error String %d", i);
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_STREQ(output.c_str(), "");
}
} // namespace SettingsReaderTests