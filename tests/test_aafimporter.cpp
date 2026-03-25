/*
 * Copyright (C) 2026 Federico Manuppella
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "AafImporter.h"
#include "IRppSink.h"
#include "LogBuffer.h"

namespace {
struct CapturingSink : IRppSink {
    std::vector<std::string> lines;
    void writeLine(const char *l) override { lines.push_back(l); }

    std::string joined() const {
        std::string out;
        for ( const auto &l : lines ) {
            out += l;
            out += '\n';
        }
        return out;
    }
};
} // namespace

TEST_CASE(

    "AafImporter golden files", "[golden]") {
    namespace fs = std::filesystem;
    const bool update = (std::getenv("REAAAF_UPDATE_GOLDEN") != nullptr);

    for ( const auto &entry : fs::directory_iterator(AAF_TEST_DIR) ) {
        if ( entry.path().extension() != ".aaf" )
            continue;
        const std::string stem = entry.path().stem().string();

        SECTION(stem) {
            CapturingSink sink;
            LogBuffer log;
            AafImporter imp(&sink, entry.path().string().c_str(), log);
            imp.run();

            const std::string actual = sink.joined();
            const fs::path goldenPath = fs::path(GOLDEN_DIR) / (stem + ".rpp");

            if ( update ) {
                fs::create_directories(GOLDEN_DIR);
                std::ofstream f(goldenPath);
                f << actual;
            } else {
                std::ifstream f(goldenPath);
                REQUIRE(f.is_open());
                const std::string expected((std::istreambuf_iterator<char>(f)),
                                           std::istreambuf_iterator<char>());

                if ( actual != expected ) {
                    // Report every differing line
                    std::istringstream actualStream(actual), expectedStream(expected);
                    std::string actualLine, expectedLine;
                    int lineNum = 0;
                    while ( true ) {
                        bool gotA = !!std::getline(actualStream, actualLine);
                        bool gotE = !!std::getline(expectedStream, expectedLine);
                        ++lineNum;
                        if ( !gotA && !gotE )
                            break;
                        if ( actualLine != expectedLine ) {
                            FAIL_CHECK("line " << lineNum
                                               << " differs\n"
                                                  "  expected: "
                                               << expectedLine
                                               << "\n"
                                                  "  actual:   "
                                               << actualLine);
                        }
                    }
                }
            }
        }
    }
}
