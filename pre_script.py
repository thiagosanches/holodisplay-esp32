Import("env")
import os

# always remove the index_html.h so that it's regenerated if index.html changes
out = os.path.join(env["PROJECT_DIR"], "src", "index_html.h")
if os.path.exists(out):
    os.remove(out)
html = os.path.join(env["PROJECT_DIR"], "index.html")
out  = os.path.join(env["PROJECT_DIR"], "src", "index_html.h")
with open(html, "r") as f:
    content = f.read()
with open(out, "w") as f:
    f.write("// Auto-generated from index.html — do not edit\n")
    f.write("#pragma once\n#include <pgmspace.h>\n")
    f.write('static const char INDEX_HTML[] PROGMEM = R"html(\n')
    f.write(content)
    f.write('\n)html";\n')

