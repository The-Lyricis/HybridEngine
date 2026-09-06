#include "project_file.h"

#include <chrono>
#include <fstream>
#include <iterator>
#include <sstream>
#include <system_error>

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif

namespace Hybrid
{
    namespace
    {
        std::string trim(std::string value)
        {
            const auto whitespace = [](unsigned char c) {
                return c == ' ' || c == '\t' || c == '\r' || c == '\n';
            };
            while (!value.empty() && whitespace(static_cast<unsigned char>(value.front())))
                value.erase(value.begin());
            while (!value.empty() && whitespace(static_cast<unsigned char>(value.back())))
                value.pop_back();
            return value;
        }

        bool replaceFile(const std::filesystem::path& temporary,
                         const std::filesystem::path& destination,
                         std::string& out_error)
        {
#ifdef _WIN32
            if (MoveFileExW(temporary.c_str(), destination.c_str(),
                            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0)
                return true;
            out_error = "failed to replace project file (Win32 error " +
                        std::to_string(GetLastError()) + "): " + destination.generic_string();
            return false;
#else
            std::error_code error;
            std::filesystem::rename(temporary, destination, error);
            if (!error)
                return true;
            out_error = "failed to replace project file: " + destination.generic_string() +
                        " (" + error.message() + ")";
            return false;
#endif
        }
    }

    bool ProjectFile::updateValue(const std::filesystem::path& project_file,
                                  const std::string& key,
                                  const std::string& value,
                                  std::string& out_error)
    {
        out_error.clear();
        if (project_file.empty() || key.empty() || key.find_first_of("=\r\n") != std::string::npos ||
            value.find_first_of("\r\n") != std::string::npos)
        {
            out_error = "invalid project update key or value";
            return false;
        }

        std::ifstream input(project_file, std::ios::binary);
        if (!input)
        {
            out_error = "failed to open project file: " + project_file.generic_string();
            return false;
        }
        const std::string content((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        if (!input.good() && !input.eof())
        {
            out_error = "failed to read project file: " + project_file.generic_string();
            return false;
        }
        input.close();

        const std::string newline = content.find("\r\n") != std::string::npos ? "\r\n" : "\n";
        std::istringstream lines(content);
        std::string line;
        std::string updated;
        bool replaced = false;
        while (std::getline(lines, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            const std::string stripped = trim(line);
            const size_t equals = stripped.find('=');
            if (!replaced && !stripped.empty() && stripped.front() != '#' && equals != std::string::npos &&
                trim(stripped.substr(0, equals)) == key)
            {
                line = key + "=" + value;
                replaced = true;
            }
            updated += line;
            updated += newline;
        }
        if (!replaced)
        {
            if (!updated.empty() && updated.size() >= newline.size() &&
                updated.compare(updated.size() - newline.size(), newline.size(), newline) != 0)
                updated += newline;
            updated += key + "=" + value + newline;
        }

        const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
        const std::filesystem::path temporary = project_file.string() + ".tmp." + std::to_string(nonce);
        {
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            if (!output)
            {
                out_error = "failed to create temporary project file: " + temporary.generic_string();
                return false;
            }
            output.write(updated.data(), static_cast<std::streamsize>(updated.size()));
            output.flush();
            if (!output)
            {
                out_error = "failed to write temporary project file: " + temporary.generic_string();
                std::error_code ignored;
                std::filesystem::remove(temporary, ignored);
                return false;
            }
        }

        if (!replaceFile(temporary, project_file, out_error))
        {
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            return false;
        }
        return true;
    }
}
