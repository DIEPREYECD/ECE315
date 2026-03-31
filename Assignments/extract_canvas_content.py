from __future__ import annotations

from dataclasses import dataclass, field
from html.parser import HTMLParser
from pathlib import Path
import re
from typing import Callable


ROOT = Path(__file__).resolve().parent
OUTPUT_DIR = ROOT / "extracted_markdown"
HTML_GLOB = "*.html"
VOID_TAGS = {
    "area",
    "base",
    "br",
    "col",
    "embed",
    "hr",
    "img",
    "input",
    "link",
    "meta",
    "param",
    "source",
    "track",
    "wbr",
}
SKIP_TAGS = {"script", "style", "svg"}
QUESTION_TYPE_LABELS = {
    "multiple_choice_question": "Multiple choice",
    "multiple_answers_question": "Select all that apply",
    "essay_question": "Written response",
    "fill_in_multiple_blanks_question": "Fill in the blanks",
}


@dataclass
class Node:
    tag: str
    attrs: dict[str, str] = field(default_factory=dict)
    children: list[object] = field(default_factory=list)
    parent: "Node | None" = None

    def append(self, child: object) -> None:
        self.children.append(child)

    def classes(self) -> set[str]:
        return {part for part in self.attrs.get("class", "").split() if part}


class DOMBuilder(HTMLParser):
    def __init__(self) -> None:
        super().__init__(convert_charrefs=True)
        self.root = Node("document")
        self.stack = [self.root]
        self.skip_depth = 0

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        if self.skip_depth:
            self.skip_depth += 1
            return

        if tag in SKIP_TAGS:
            self.skip_depth = 1
            return

        node = Node(tag, {key: value or "" for key, value in attrs}, parent=self.stack[-1])
        self.stack[-1].append(node)
        if tag not in VOID_TAGS:
            self.stack.append(node)

    def handle_startendtag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        if self.skip_depth or tag in SKIP_TAGS:
            return
        node = Node(tag, {key: value or "" for key, value in attrs}, parent=self.stack[-1])
        self.stack[-1].append(node)

    def handle_endtag(self, tag: str) -> None:
        if self.skip_depth:
            self.skip_depth -= 1
            return

        for index in range(len(self.stack) - 1, 0, -1):
            if self.stack[index].tag == tag:
                del self.stack[index:]
                return

    def handle_data(self, data: str) -> None:
        if self.skip_depth or not data:
            return
        self.stack[-1].append(data)


def iter_nodes(node: Node):
    for child in node.children:
        if isinstance(child, Node):
            yield child
            yield from iter_nodes(child)


def find_first(node: Node, predicate: Callable[[Node], bool]) -> Node | None:
    if predicate(node):
        return node
    for child in node.children:
        if isinstance(child, Node):
            found = find_first(child, predicate)
            if found is not None:
                return found
    return None


def find_all(node: Node, predicate: Callable[[Node], bool]) -> list[Node]:
    matches = []
    if predicate(node):
        matches.append(node)
    for child in node.children:
        if isinstance(child, Node):
            matches.extend(find_all(child, predicate))
    return matches


def has_classes(node: Node, *names: str) -> bool:
    classes = node.classes()
    return all(name in classes for name in names)


def direct_children(node: Node, tag: str | None = None) -> list[Node]:
    children = []
    for child in node.children:
        if isinstance(child, Node) and (tag is None or child.tag == tag):
            children.append(child)
    return children


def text_content(node: Node) -> str:
    parts: list[str] = []
    for child in node.children:
        if isinstance(child, Node):
            if "screenreader-only" in child.classes() or "external_link_icon" in child.classes():
                continue
            parts.append(text_content(child))
        else:
            parts.append(child)
    text = "".join(parts).replace("\xa0", " ")
    return re.sub(r"\s+", " ", text).strip()


def render_children(node: Node, indent: int = 0) -> str:
    return "".join(render_node(child, indent) for child in node.children)


def render_inline(node: Node) -> str:
    return collapse_inline(render_node(node, 0))


def collapse_inline(text: str) -> str:
    text = text.replace("\xa0", " ")
    text = text.replace("\r", "")
    text = re.sub(r"[ \t]+", " ", text)
    text = re.sub(r" *\n+ *", " ", text)
    return text.strip()


def indent_block(text: str, prefix: str) -> str:
    lines = text.rstrip().splitlines()
    return "\n".join(f"{prefix}{line}" if line else prefix.rstrip() for line in lines)


def render_list(node: Node, ordered: bool, indent: int) -> str:
    lines: list[str] = []
    item_index = 1
    for child in direct_children(node, "li"):
        prefix = f"{item_index}. " if ordered else "- "
        lines.extend(render_list_item(child, prefix, indent))
        item_index += 1
    return "\n".join(lines).rstrip() + "\n\n"


def render_list_item(node: Node, prefix: str, indent: int) -> list[str]:
    inline_parts: list[str] = []
    nested_blocks: list[str] = []

    for child in node.children:
        if isinstance(child, Node) and child.tag in {"ul", "ol"}:
            nested_blocks.append(render_node(child, indent + 1).rstrip())
        else:
            rendered = render_node(child, indent).strip()
            if rendered:
                inline_parts.append(rendered)

    line = f"{'  ' * indent}{prefix}{collapse_inline(' '.join(inline_parts))}".rstrip()
    lines = [line]
    for block in nested_blocks:
        lines.append(indent_block(block, "  " * (indent + 1)))
    return lines


def render_node(item: object, indent: int = 0) -> str:
    if isinstance(item, str):
        text = item.replace("\xa0", " ")
        return re.sub(r"\s+", " ", text)

    node = item
    if "screenreader-only" in node.classes() or "external_link_icon" in node.classes():
        return ""

    tag = node.tag
    if tag in {"div", "section", "article", "span"}:
        return render_children(node, indent)
    if tag == "p":
        text = collapse_inline(render_children(node, indent))
        return f"{text}\n\n" if text else ""
    if tag in {"h1", "h2", "h3", "h4"}:
        level = int(tag[1])
        title = collapse_inline(render_children(node, indent))
        if not title:
            return ""
        level = min(level + 1, 6)
        return f"{'#' * level} {title}\n\n"
    if tag == "strong" or tag == "b":
        text = collapse_inline(render_children(node, indent))
        return f"**{text}**" if text else ""
    if tag == "em" or tag == "i":
        text = collapse_inline(render_children(node, indent))
        return f"*{text}*" if text else ""
    if tag == "code":
        text = collapse_inline(render_children(node, indent))
        if not text:
            return ""
        return f"`{text}`"
    if tag == "sub":
        text = collapse_inline(render_children(node, indent))
        return f"<sub>{text}</sub>" if text else ""
    if tag == "sup":
        text = collapse_inline(render_children(node, indent))
        return f"<sup>{text}</sup>" if text else ""
    if tag == "br":
        return "\n"
    if tag == "a":
        href = node.attrs.get("href", "").strip()
        label = collapse_inline(render_children(node, indent)) or href
        if not href:
            return label
        return f"[{label}]({href})"
    if tag == "img":
        src = node.attrs.get("src", "").strip()
        alt = node.attrs.get("alt", "").strip() or "image"
        if src.endswith("/preview") or src.endswith("\\preview") or Path(src).name == "preview":
            return ""
        if src.startswith("./"):
            src = "../" + src[2:]
        return f"![{alt}]({src})"
    if tag == "input":
        if node.attrs.get("class", "").find("question_input") >= 0:
            return "[blank]"
        return ""
    if tag == "ul":
        return render_list(node, ordered=False, indent=indent)
    if tag == "ol":
        return render_list(node, ordered=True, indent=indent)
    if tag == "li":
        return "\n".join(render_list_item(node, "- ", indent)) + "\n"
    if tag == "textarea":
        return ""
    return render_children(node, indent)


def cleanup_markdown(text: str) -> str:
    text = text.replace("\r", "")
    text = re.sub(r"[ \t]+\n", "\n", text)
    text = re.sub(r"(?m)^ (?=(?:[-*]|\d+\.)\s)", "", text)
    text = re.sub(r"(?m)^( +)(?!(?:[-*]|\d+\.)\s)", "", text)
    text = re.sub(r"\n{3,}", "\n\n", text)
    return text.strip() + "\n"


def title_from_dom(root: Node, fallback: str) -> str:
    title_node = find_first(root, lambda n: n.tag == "title")
    if title_node is None:
        return fallback
    title = text_content(title_node)
    return title or fallback


def instructions_markdown(root: Node) -> str:
    node = find_first(root, lambda n: n.tag == "div" and has_classes(n, "description", "user_content", "enhanced"))
    if node is None:
        return ""
    rendered = cleanup_markdown(render_children(node))
    return rendered


def extract_questions(root: Node) -> list[dict[str, object]]:
    questions: list[dict[str, object]] = []
    for node in find_all(root, lambda n: n.tag == "div" and "display_question" in n.classes()):
        question_name = find_first(node, lambda n: n.tag == "span" and "question_name" in n.classes())
        question_type = find_first(node, lambda n: n.tag == "span" and "question_type" in n.classes())
        prompt = find_first(node, lambda n: n.tag == "div" and has_classes(n, "question_text", "user_content", "enhanced"))

        if prompt is None:
            continue

        answers: list[str] = []
        if question_type is not None and text_content(question_type) in {
            "multiple_choice_question",
            "multiple_answers_question",
        }:
            answers_container = find_first(node, lambda n: n.tag == "div" and "answers_wrapper" in n.classes())
            if answers_container is not None:
                for answer_node in direct_children(answers_container, "div"):
                    if "answer" not in answer_node.classes():
                        continue
                    label = find_first(answer_node, lambda n: n.tag == "label")
                    if label is None:
                        continue
                    rendered = cleanup_markdown(render_children(label))
                    if rendered:
                        answers.append(rendered.strip())

        questions.append(
            {
                "name": text_content(question_name) or f"Question {len(questions) + 1}",
                "type": text_content(question_type),
                "prompt": cleanup_markdown(render_children(prompt)),
                "answers": answers,
            }
        )

    return questions


def question_markdown(question: dict[str, object]) -> str:
    name = str(question["name"])
    question_type = str(question["type"])
    prompt = str(question["prompt"])
    answers = list(question["answers"])

    lines = [f"## {name}", ""]
    label = QUESTION_TYPE_LABELS.get(question_type)
    if label:
        lines.append(f"_Type: {label}_")
        lines.append("")

    lines.append(prompt.rstrip())
    lines.append("")

    if answers:
        if question_type == "multiple_choice_question":
            lines.append("Options:")
        elif question_type == "multiple_answers_question":
            lines.append("Options:")
        else:
            lines.append("Responses:")
        lines.append("")
        for answer in answers:
            bullet = indent_block(answer.strip(), "  ").lstrip()
            lines.append(f"- {bullet}")
        lines.append("")

    return "\n".join(lines).rstrip() + "\n"


def output_name(html_path: Path) -> str:
    return html_path.stem + ".md"


def build_markdown(html_path: Path) -> str:
    parser = DOMBuilder()
    parser.feed(html_path.read_text(encoding="utf-8", errors="ignore"))
    root = parser.root

    title = title_from_dom(root, html_path.stem)
    instructions = instructions_markdown(root)
    questions = extract_questions(root)

    parts = [
        f"# {title}",
        "",
        "This export keeps the assignment or quiz prompt content, instructions, answer choices, and embedded figures.",
        "It omits your submitted answers, attempt history, and score details.",
        "",
        f"Source HTML: `{html_path.name}`",
        "",
    ]

    if instructions:
        parts.extend(["## Instructions", "", instructions.rstrip(), ""])

    parts.extend(["## Questions", ""])
    for question in questions:
        parts.append(question_markdown(question).rstrip())
        parts.append("")

    return cleanup_markdown("\n".join(parts))


def build_index(entries: list[tuple[str, str]]) -> str:
    lines = [
        "# ECE 315 Extracted Assignment and Quiz Content",
        "",
        "Markdown exports generated from the saved Canvas HTML pages in this folder.",
        "",
    ]
    for title, filename in entries:
        lines.append(f"- [{title}]({filename})")
    lines.append("")
    return "\n".join(lines)


def main() -> None:
    OUTPUT_DIR.mkdir(exist_ok=True)
    entries: list[tuple[str, str]] = []

    for html_path in sorted(ROOT.glob(HTML_GLOB)):
        markdown = build_markdown(html_path)
        md_name = output_name(html_path)
        (OUTPUT_DIR / md_name).write_text(markdown, encoding="utf-8")
        entries.append((html_path.stem, md_name))

    (OUTPUT_DIR / "README.md").write_text(build_index(entries), encoding="utf-8")
    print(f"Wrote {len(entries)} markdown files to {OUTPUT_DIR}")


if __name__ == "__main__":
    main()
