import re
import sys
from pathlib import Path
from typing import List, Iterator


def _wclen(text: str) -> int:
    ret = 0
    for char in text:
        if ord(char) < 128:
            ret += 1
        else:
            ret += 2
    return ret


def decode_script(content: bytes) -> bytes:
    def format_command(command: str, arg: str) -> str:
        return '{} {}'.format(command, arg or '').rstrip()

    def process_line(line: str) -> List[str]:
        if not line:
            return ['']

        match = re.match('^【】(?P<text>.*)$', line)
        if match:
            ret = []
            ret.append(format_command('TEXT', match.group('name')))
            return ret

        match = re.match(
            '^【(?P<name>[^,]*)(?:,(?P<sound>.+))?】(?P<text>.*)$',
            line)
        if match:
            ret = []
            ret.append(format_command('SPEAK-CHAR', match.group('name')))
            if match.group('sound'):
                ret.append(format_command('SPEAK-FILE', match.group('sound')))
            ret.append(format_command('SPEAK-ORIG', match.group('text')))
            ret.append(format_command('SPEAK-TEXT', match.group('text')))
            return ret

        if ord(line[0]) < 128:
            return [format_command('CODE', line)]

        return [
            format_command('ORIG', line),
            format_command('TEXT', line),
        ]

    lines = [
        new_line
        for old_line in content.decode('cp932').split('\n')
        for new_line in process_line(old_line.strip())
    ]
    return '\n'.join(lines).encode('utf-8')


def encode_script(
        script_path: Path,
        content: bytes,
        max_line_count: int,
        max_line_length: int) -> bytes:
    def process_text(
            input_string: str,
            line_number: int) -> str:
        NEWLINE_MARKER = '[n]'

        prefix = ''
        suffix = ''
        while input_string.startswith(('「', '『', '（')):
            prefix += input_string[0:1]
            input_string = input_string[1:]
        while input_string.endswith(('）', '』', '」')):
            suffix = input_string[-1] + suffix
            input_string = input_string[0:-1]

        output_lines = []
        for input_line in input_string.split(NEWLINE_MARKER):
            if not input_line:
                output_lines.append('')
                continue
            output_words = []
            for word in input_line.split():
                if _wclen(' '.join(output_words + [word])) <= max_line_length:
                    output_words.append(word)
                else:
                    if output_words:
                        output_lines.append(' '.join(output_words))
                    output_words = [word]
            if output_words:
                output_lines.append(' '.join(output_words))
        output_string = NEWLINE_MARKER.join(output_lines)

        if len(output_lines) > max_line_count:
            print(
                'Warning: too many lines in {} at line {}'.format(
                    script_path, line_number),
                file=sys.stderr)

        if any(_wclen(line) > max_line_length for line in output_lines):
            print(
                'Warning: too long line in {} at line {}'.format(
                    script_path, line_number),
                file=sys.stderr)

        return prefix + output_string + suffix

    def gather_lines() -> Iterator[str]:
        text = {}
        speech = {}

        for i, old_line in enumerate(content.decode('utf-8').split('\n')):
            line_number = i + 1
            if not old_line:
                yield ''
                continue

            command, arg = (
                old_line.lstrip().split(' ', 1)
                if ' ' in old_line
                else (old_line, ''))

            if command == 'SPEAK-CHAR':
                speech['char'] = arg
            elif command == 'SPEAK-FILE':
                speech['file'] = arg
            elif command == 'SPEAK-ORIG':
                speech['orig'] = arg
            elif command == 'SPEAK-TEXT':
                if 'orig' in speech and speech['orig'] == arg:
                    speech['text'] = speech['orig']
                else:
                    speech['text'] = process_text(arg, line_number)
                if 'file' in speech:
                    yield '【{char},{file}】{text}'.format(**speech)
                else:
                    yield '【{char}】{text}'.format(**speech)
                speech = {}

            elif command == 'ORIG':
                text['orig'] = arg
            elif command == 'TEXT':
                if 'orig' in text and text['orig'] == arg:
                    text['text'] = text['orig']
                else:
                    text['text'] = process_text(arg, line_number)
                yield '【】' + text['text']
                text = {}

            elif command == 'CODE':
                yield arg

    return '\n'.join(gather_lines()).encode('cp932')
