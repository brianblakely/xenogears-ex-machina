"""Centre of the first node of a uiautomator dump whose text or content-desc
matches a regular expression, as "x y" (exit status 1 if none does).

    python3 runtime/scripts/android-ui.py dump.xml 'xem-synthetic\\.bin'
"""

import re
import sys
import xml.etree.ElementTree as ElementTree


def main() -> int:
    path, pattern = sys.argv[1], re.compile(sys.argv[2])
    for node in ElementTree.parse(path).iter("node"):
        label = node.get("text") or node.get("content-desc") or ""
        if pattern.fullmatch(label):
            left, top, right, bottom = map(int, re.findall(r"\d+", node.get("bounds", "")))
            print((left + right) // 2, (top + bottom) // 2)
            return 0
    return 1


if __name__ == "__main__":
    sys.exit(main())
