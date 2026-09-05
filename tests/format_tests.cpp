#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "format/json_value.h"
#include "format/parser.h"
#include "format/writer.h"

using regui::format::JsonType;
using regui::format::JsonValue;
using regui::format::parseDocument;
using regui::format::writeDocument;

namespace {

int gFailures = 0;

void check(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        ++gFailures;
    }
}

std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Could not open " + path);
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

void testRelayoutExample(const std::string& examplesDir) {
    std::string text = readFile(examplesDir + "/example.relayout");
    JsonValue root = parseDocument(text);

    check(root.isObject(), "relayout root is an object");
    const JsonValue* version = root.find("version");
    check(version != nullptr && version->isNumber() && version->numberValue == 3, "relayout version == 3");

    const JsonValue* metadata = root.find("metadata");
    check(metadata != nullptr && metadata->find("name") != nullptr &&
              metadata->find("name")->stringValue == "MyReLayout",
          "relayout metadata.name");

    const JsonValue* data = root.find("data");
    check(data != nullptr && data->isArray() && data->arrayItems.size() == 1, "relayout data has one widget");
    if (!data || data->arrayItems.empty()) {
        return;
    }

    const JsonValue& widget = data->arrayItems[0];
    check(widget.find("name") != nullptr && widget.find("name")->stringValue == "MyWidget", "widget name");
    check(widget.find("type") != nullptr && widget.find("type")->stringValue == "ReGui.Widget", "widget type");

    const JsonValue* properties = widget.find("properties");
    check(properties != nullptr, "widget has properties");
    const JsonValue* layout = properties->find("layout");
    check(layout != nullptr, "properties has layout");
    const JsonValue* anchor = layout->find("anchor");
    check(anchor != nullptr && anchor->find("x")->numberValue == 0.5 && anchor->find("y")->numberValue == 0.5,
          "layout anchor is (0.5, 0.5)");

    const JsonValue* width = layout->find("width");
    check(width != nullptr && width->find("scale")->numberValue == 0.5, "layout width.scale == 0.5");

    check(properties->find("skin") != nullptr && properties->find("skin")->stringValue == "<skin name>",
          "properties.skin");
    const JsonValue* callbacks = properties->find("callbacks");
    check(callbacks != nullptr && callbacks->find("onClick")->stringValue == "cl_onWidgetClick",
          "callbacks.onClick");
}

void testReaddonExample(const std::string& examplesDir) {
    std::string text = readFile(examplesDir + "/example.readdon");
    JsonValue root = parseDocument(text);

    const JsonValue* widgets = root.find("widgets");
    check(widgets != nullptr && widgets->isArray() && widgets->arrayItems.size() == 1, "readdon has one widget def");
    if (!widgets || widgets->arrayItems.empty()) {
        return;
    }

    const JsonValue& widget = widgets->arrayItems[0];
    check(widget.find("name")->stringValue == "MyCustomWidget", "widget def name");

    const JsonValue* availableProps = widget.find("available-properties");
    check(availableProps != nullptr, "widget def has available-properties");
    if (!availableProps) {
        return;
    }

    // MyProperty is a definition object carrying both its default value and metadata,
    // written as plain JSON fields rather than a comment-embedded mini-language.
    const JsonValue* myProperty = availableProps->find("MyProperty");
    check(myProperty != nullptr && myProperty->isObject(), "MyProperty is a definition object");
    if (myProperty) {
        const std::string* defaultValue = myProperty->getString("default");
        const std::string* label = myProperty->getString("label");
        const std::string* description = myProperty->getString("description");
        check(defaultValue != nullptr && *defaultValue == "<default value>", "MyProperty default value");
        check(label != nullptr && *label == "Additional Property", "MyProperty metadata label");
        check(description != nullptr && *description == "An additional property that does smth.",
              "MyProperty metadata description");
    }

    const JsonValue* myOtherProperty = availableProps->find("MyOtherProperty");
    check(myOtherProperty != nullptr && myOtherProperty->isObject() &&
              myOtherProperty->getString("default") != nullptr && myOtherProperty->getString("type") != nullptr &&
              *myOtherProperty->getString("type") == "integer" &&
              myOtherProperty->getString("parent") != nullptr && *myOtherProperty->getString("parent") == "MyProperty",
          "MyOtherProperty carries a default value, type and parent");

    const JsonValue* requiredCallbacks = widget.find("required-callbacks");
    check(requiredCallbacks != nullptr && requiredCallbacks->find("onSomething") != nullptr,
          "required-callbacks.onSomething present");

    const JsonValue* availableCallbacks = widget.find("available-callbacks");
    check(availableCallbacks != nullptr && availableCallbacks->isObject() &&
              availableCallbacks->find("onSomething") != nullptr && availableCallbacks->find("onSomething2") != nullptr,
          "available-callbacks object contents");
}

void testWriteReparseRoundTrip(const std::string& examplesDir, const std::string& filename) {
    std::string text = readFile(examplesDir + "/" + filename);
    JsonValue original = parseDocument(text);
    std::string written = writeDocument(original);
    JsonValue reparsed = parseDocument(written);
    check(original == reparsed, filename + ": parse -> write -> parse is structurally identical");
}

void testSyntheticMetadataThroughWriter() {
    JsonValue root = JsonValue::makeObject();
    JsonValue prop = JsonValue::makeObject();
    prop.set("default", JsonValue::makeNumber(42));
    prop.set("label", JsonValue::makeString("Count"));
    prop.set("description", JsonValue::makeString("How many, in this format: zero."));
    root.set("count", prop);

    std::string written = writeDocument(root);
    JsonValue reparsed = parseDocument(written);
    const JsonValue* reparsedProp = reparsed.find("count");
    check(reparsedProp != nullptr && reparsedProp->isObject(), "synthetic definition round-trips");
    if (reparsedProp) {
        const JsonValue* defaultValue = reparsedProp->find("default");
        const std::string* label = reparsedProp->getString("label");
        check(defaultValue != nullptr && defaultValue->isNumber() && defaultValue->numberValue == 42,
              "synthetic default round-trips through writer");
        check(label != nullptr && *label == "Count", "synthetic metadata round-trips through writer");
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: format_tests <examples-dir>\n";
        return 2;
    }
    std::string examplesDir = argv[1];

    try {
        testRelayoutExample(examplesDir);
        testReaddonExample(examplesDir);
        testWriteReparseRoundTrip(examplesDir, "example.relayout");
        testWriteReparseRoundTrip(examplesDir, "example.readdon");
        testSyntheticMetadataThroughWriter();
    } catch (const std::exception& e) {
        std::cerr << "FAIL: unexpected exception: " << e.what() << "\n";
        ++gFailures;
    }

    if (gFailures == 0) {
        std::cout << "All format tests passed.\n";
        return 0;
    }
    std::cerr << gFailures << " test(s) failed.\n";
    return 1;
}
