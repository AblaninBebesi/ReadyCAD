#pragma once

// A few checks of a PDF file for tests. ReadyCAD writes pages uncompressed, so page objects and
// texts can be found in the bytes without a PDF library.

#include <string>

namespace rc::test {

inline std::size_t occurrences(const std::string& text, const std::string& what)
{
    std::size_t n = 0;
    for (std::size_t pos = text.find(what); pos != std::string::npos;
         pos = text.find(what, pos + 1)) {
        ++n;
    }
    return n;
}

/// Page objects (not the /Pages tree node).
inline std::size_t pdfPageCount(const std::string& pdf)
{
    return occurrences(pdf, "/Type /Page\n") + occurrences(pdf, "/Type /Page\r")
         + occurrences(pdf, "/Type /Page/") + occurrences(pdf, "/Type /Page>");
}

/// Starts like a PDF and ends with the end-of-file marker: written completely.
inline bool isCompletePdf(const std::string& pdf)
{
    return pdf.rfind("%PDF-", 0) == 0 && pdf.find("%%EOF", pdf.size() > 64 ? pdf.size() - 64 : 0)
                                             != std::string::npos;
}

} // namespace rc::test
