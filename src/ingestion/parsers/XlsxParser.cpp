#include "XlsxParser.h"

#include <zip.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

namespace ingestion::parsers
{

namespace
{

// -------------------------------------------------------------------------
// Zip helper
// -------------------------------------------------------------------------
std::string readZipEntry(zip_t *archive, const char *entryName)
{
    zip_int64_t index = zip_name_locate(archive, entryName, 0);
    if (index < 0)
        return "";

    zip_file_t *file = zip_fopen_index(archive, index, 0);
    if (!file)
        return "";

    std::string content;
    char        buffer[4096];
    zip_int64_t bytesRead;

    while ((bytesRead = zip_fread(file, buffer, sizeof(buffer))) > 0)
        content.append(buffer, static_cast<std::size_t>(bytesRead));

    zip_fclose(file);
    return content;
}

// -------------------------------------------------------------------------
// Shared-string table
// -------------------------------------------------------------------------
std::vector<std::string> buildSharedStringTable(const std::string &sharedStringsXml)
{
    std::vector<std::string> table;

    if (sharedStringsXml.empty())
        return table;

    xmlDocPtr doc = xmlReadMemory(
        sharedStringsXml.data(),
        static_cast<int>(sharedStringsXml.size()),
        "sharedStrings.xml",
        nullptr,
        XML_PARSE_RECOVER | XML_PARSE_NOERROR | XML_PARSE_NOWARNING);

    if (!doc)
        return table;

    // Walk all <si> elements; concatenate all <t> text children.
    xmlNodePtr root = xmlDocGetRootElement(doc);
    for (xmlNodePtr si = root ? root->children : nullptr;
         si != nullptr;
         si = si->next)
    {
        if (si->type != XML_ELEMENT_NODE)
            continue;

        std::string entry;
        // Each <si> may contain one or more <r><t>...</t></r> (rich text)
        // or a direct <t>...</t>.
        for (xmlNodePtr child = si->children; child; child = child->next)
        {
            if (child->type != XML_ELEMENT_NODE)
                continue;

            const std::string childName =
                reinterpret_cast<const char *>(child->name);

            if (childName == "t")
            {
                xmlChar *content = xmlNodeGetContent(child);
                if (content)
                {
                    entry += reinterpret_cast<const char *>(content);
                    xmlFree(content);
                }
            }
            else if (childName == "r")
            {
                // Rich-text run: find nested <t>
                for (xmlNodePtr t = child->children; t; t = t->next)
                {
                    if (t->type == XML_ELEMENT_NODE &&
                        std::string(reinterpret_cast<const char *>(t->name)) == "t")
                    {
                        xmlChar *content = xmlNodeGetContent(t);
                        if (content)
                        {
                            entry += reinterpret_cast<const char *>(content);
                            xmlFree(content);
                        }
                    }
                }
            }
        }
        table.push_back(std::move(entry));
    }

    xmlFreeDoc(doc);
    return table;
}

// -------------------------------------------------------------------------
// Parse a single sheet XML into ParsedUnits
// -------------------------------------------------------------------------
void parseSheetXml(
    const std::string              &sheetXml,
    const std::string              &sheetName,
    const std::vector<std::string> &sharedStrings,
    std::vector<ParsedUnit>        &units)
{
    if (sheetXml.empty())
        return;

    xmlDocPtr doc = xmlReadMemory(
        sheetXml.data(),
        static_cast<int>(sheetXml.size()),
        "sheet.xml",
        nullptr,
        XML_PARSE_RECOVER | XML_PARSE_NOERROR | XML_PARSE_NOWARNING);

    if (!doc)
        return;

    xmlNodePtr root = xmlDocGetRootElement(doc);
    if (!root)
    {
        xmlFreeDoc(doc);
        return;
    }

    // Find <sheetData> child
    xmlNodePtr sheetData = nullptr;
    for (xmlNodePtr n = root->children; n; n = n->next)
    {
        if (n->type == XML_ELEMENT_NODE)
        {
            const std::string name(reinterpret_cast<const char *>(n->name));
            if (name == "sheetData")
            {
                sheetData = n;
                break;
            }
        }
    }

    if (!sheetData)
    {
        xmlFreeDoc(doc);
        return;
    }

    int rowNum = 0;

    for (xmlNodePtr rowNode = sheetData->children; rowNode; rowNode = rowNode->next)
    {
        if (rowNode->type != XML_ELEMENT_NODE)
            continue;

        const std::string rowName(reinterpret_cast<const char *>(rowNode->name));
        if (rowName != "row")
            continue;

        ++rowNum;

        // Get Excel row index attribute for accurate source_location
        xmlChar *rAttr = xmlGetProp(rowNode, reinterpret_cast<const xmlChar *>("r"));
        int excelRow = rowNum;
        if (rAttr)
        {
            try { excelRow = std::stoi(reinterpret_cast<const char *>(rAttr)); }
            catch (...) {}
            xmlFree(rAttr);
        }

        std::string rowText;
        int         colNum = 0;

        for (xmlNodePtr cellNode = rowNode->children; cellNode; cellNode = cellNode->next)
        {
            if (cellNode->type != XML_ELEMENT_NODE)
                continue;

            const std::string cellName(reinterpret_cast<const char *>(cellNode->name));
            if (cellName != "c")
                continue;

            ++colNum;

            // t attribute: "s" = shared string index, "inlineStr", else numeric/formula
            xmlChar *typeAttr = xmlGetProp(
                cellNode, reinterpret_cast<const xmlChar *>("t"));
            const std::string cellType =
                typeAttr ? reinterpret_cast<const char *>(typeAttr) : "";
            if (typeAttr) xmlFree(typeAttr);

            std::string cellValue;

            for (xmlNodePtr vNode = cellNode->children; vNode; vNode = vNode->next)
            {
                if (vNode->type != XML_ELEMENT_NODE)
                    continue;

                const std::string vName(
                    reinterpret_cast<const char *>(vNode->name));

                if (vName == "v")
                {
                    xmlChar *content = xmlNodeGetContent(vNode);
                    if (content)
                    {
                        const std::string raw(reinterpret_cast<const char *>(content));
                        xmlFree(content);

                        if (cellType == "s")
                        {
                            // Shared string index
                            try
                            {
                                const std::size_t idx =
                                    static_cast<std::size_t>(std::stoi(raw));
                                if (idx < sharedStrings.size())
                                    cellValue = sharedStrings[idx];
                                else
                                    cellValue = raw;
                            }
                            catch (...)
                            {
                                cellValue = raw;
                            }
                        }
                        else
                        {
                            cellValue = raw;
                        }
                    }
                }
                else if (vName == "is") // inline string
                {
                    for (xmlNodePtr t = vNode->children; t; t = t->next)
                    {
                        if (t->type == XML_ELEMENT_NODE &&
                            std::string(reinterpret_cast<const char *>(t->name)) == "t")
                        {
                            xmlChar *content = xmlNodeGetContent(t);
                            if (content)
                            {
                                cellValue += reinterpret_cast<const char *>(content);
                                xmlFree(content);
                            }
                        }
                    }
                }
            }

            if (!cellValue.empty())
            {
                if (!rowText.empty())
                    rowText += "\t";
                rowText += cellValue;
            }
        }

        if (rowText.empty())
            continue; // skip blank rows

        ParsedUnit unit;
        unit.text           = rowText;
        unit.sourceLocation = "sheet '" + sheetName + "' row " +
                              std::to_string(excelRow);
        units.push_back(std::move(unit));
    }

    xmlFreeDoc(doc);
}

// -------------------------------------------------------------------------
// Read sheet names from workbook.xml
// -------------------------------------------------------------------------
std::vector<std::pair<std::string, std::string>> parseSheetList(
    const std::string &workbookXml)
{
    // Returns vector of {sheetName, relationshipId}
    std::vector<std::pair<std::string, std::string>> sheets;

    if (workbookXml.empty())
        return sheets;

    xmlDocPtr doc = xmlReadMemory(
        workbookXml.data(),
        static_cast<int>(workbookXml.size()),
        "workbook.xml",
        nullptr,
        XML_PARSE_RECOVER | XML_PARSE_NOERROR | XML_PARSE_NOWARNING);

    if (!doc)
        return sheets;

    xmlNodePtr root = xmlDocGetRootElement(doc);
    for (xmlNodePtr node = root ? root->children : nullptr;
         node;
         node = node->next)
    {
        if (node->type != XML_ELEMENT_NODE)
            continue;

        if (std::string(reinterpret_cast<const char *>(node->name)) == "sheets")
        {
            for (xmlNodePtr s = node->children; s; s = s->next)
            {
                if (s->type != XML_ELEMENT_NODE)
                    continue;

                xmlChar *nameAttr = xmlGetProp(
                    s, reinterpret_cast<const xmlChar *>("name"));
                xmlChar *ridAttr  = xmlGetProp(
                    s, reinterpret_cast<const xmlChar *>("id"));

                std::string shName = nameAttr
                    ? reinterpret_cast<const char *>(nameAttr) : "Sheet";
                std::string rId   = ridAttr
                    ? reinterpret_cast<const char *>(ridAttr)  : "";

                if (nameAttr) xmlFree(nameAttr);
                if (ridAttr)  xmlFree(ridAttr);

                sheets.emplace_back(std::move(shName), std::move(rId));
            }
            break;
        }
    }

    xmlFreeDoc(doc);
    return sheets;
}

// -------------------------------------------------------------------------
// Resolve rId → sheet file path via xl/_rels/workbook.xml.rels
// -------------------------------------------------------------------------
std::string resolveSheetPath(const std::string &relsXml, const std::string &rId)
{
    if (relsXml.empty() || rId.empty())
        return "";

    xmlDocPtr doc = xmlReadMemory(
        relsXml.data(),
        static_cast<int>(relsXml.size()),
        "workbook.xml.rels",
        nullptr,
        XML_PARSE_RECOVER | XML_PARSE_NOERROR | XML_PARSE_NOWARNING);

    if (!doc)
        return "";

    std::string target;
    xmlNodePtr  root = xmlDocGetRootElement(doc);

    for (xmlNodePtr rel = root ? root->children : nullptr; rel; rel = rel->next)
    {
        if (rel->type != XML_ELEMENT_NODE)
            continue;

        xmlChar *id = xmlGetProp(rel, reinterpret_cast<const xmlChar *>("Id"));
        if (!id)
            continue;

        if (rId == reinterpret_cast<const char *>(id))
        {
            xmlFree(id);
            xmlChar *tgt = xmlGetProp(
                rel, reinterpret_cast<const xmlChar *>("Target"));
            if (tgt)
            {
                target = "xl/" +
                    std::string(reinterpret_cast<const char *>(tgt));
                xmlFree(tgt);
            }
            break;
        }
        xmlFree(id);
    }

    xmlFreeDoc(doc);
    return target;
}

} // namespace

ParseResult XlsxParser::parse(const std::string &fileBytes)
{
    ParseResult result;

    if (fileBytes.empty())
    {
        result.errorCode    = "EMPTY_DOCUMENT";
        result.errorMessage = "XLSX file is empty";
        return result;
    }

    // Magic-byte check: XLSX is a ZIP (PK header 50 4B 03 04)
    if (fileBytes.size() < 4 ||
        static_cast<unsigned char>(fileBytes[0]) != 0x50 ||
        static_cast<unsigned char>(fileBytes[1]) != 0x4B ||
        static_cast<unsigned char>(fileBytes[2]) != 0x03 ||
        static_cast<unsigned char>(fileBytes[3]) != 0x04)
    {
        result.errorCode    = "PARSE_FAILURE";
        result.errorMessage = "File does not have a valid XLSX/ZIP signature";
        return result;
    }

    zip_error_t zipError;
    zip_error_init(&zipError);

    zip_source_t *source = zip_source_buffer_create(
        fileBytes.data(), fileBytes.size(), 0, &zipError);

    if (!source)
    {
        zip_error_fini(&zipError);
        result.errorCode    = "PARSE_FAILURE";
        result.errorMessage = "Unable to read XLSX archive";
        return result;
    }

    zip_t *archive = zip_open_from_source(source, ZIP_RDONLY, &zipError);
    if (!archive)
    {
        zip_source_free(source);
        zip_error_fini(&zipError);
        result.errorCode    = "PARSE_FAILURE";
        result.errorMessage = "Invalid XLSX archive";
        return result;
    }

    const std::string workbookXml    = readZipEntry(archive, "xl/workbook.xml");
    const std::string sharedStrXml   = readZipEntry(archive, "xl/sharedStrings.xml");
    const std::string relsXml        = readZipEntry(archive, "xl/_rels/workbook.xml.rels");

    if (workbookXml.empty())
    {
        zip_close(archive);
        zip_error_fini(&zipError);
        result.errorCode    = "PARSE_FAILURE";
        result.errorMessage = "Invalid XLSX workbook structure";
        return result;
    }

    const auto sharedStrings = buildSharedStringTable(sharedStrXml);
    const auto sheetList     = parseSheetList(workbookXml);

    if (sheetList.empty())
    {
        zip_close(archive);
        zip_error_fini(&zipError);
        result.errorCode    = "PARSE_FAILURE";
        result.errorMessage = "No sheets found in XLSX workbook";
        return result;
    }

    // Process all sheets (multi-sheet TBD behaviour: ingest all by default)
    for (const auto &[sheetName, rId] : sheetList)
    {
        std::string sheetPath = resolveSheetPath(relsXml, rId);
        if (sheetPath.empty())
        {
            // Fallback: try sheet1, sheet2 … by index
            sheetPath = "xl/worksheets/sheet1.xml";
        }

        const std::string sheetXml = readZipEntry(archive, sheetPath.c_str());
        parseSheetXml(sheetXml, sheetName, sharedStrings, result.units);
    }

    zip_close(archive);
    zip_error_fini(&zipError);

    if (result.units.empty())
    {
        result.errorCode    = "EMPTY_DOCUMENT";
        result.errorMessage = "XLSX file contains no readable data";
        return result;
    }

    result.success = true;
    return result;
}

} // namespace ingestion::parsers
