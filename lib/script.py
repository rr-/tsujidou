import re
from typing import List, Iterator


def decode_script(content: bytes) -> bytes:
    def format_command(command: str, arg: str) -> str:
        return '{} {}'.format(command, arg or '').rstrip()

    def process_line(line: str) -> List[str]:
        match = re.match(
            '【(?P<name>[^,]*)(?:,(?P<sound>.+))?】(?P<text>.*)',
            line)
        if match:
            ret = []
            ret.append(format_command('SPEAK-CHAR', match.group('name')))
            if match.group('sound'):
                ret.append(format_command('SPEAK-FILE', match.group('sound')))
            ret.append(format_command('SPEAK-ORIG', match.group('text')))
            ret.append(format_command('SPEAK-TEXT', match.group('text')))
            return ret

        if not line:
            return ['']

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


def encode_script(content: bytes) -> bytes:
    def gather_lines() -> Iterator[str]:
        speech = {}

        for old_line in content.decode('utf-8').split('\n'):
            if not old_line:
                yield ''
                continue

            command, args = (
                old_line.lstrip().split(' ', 1)
                if ' ' in old_line
                else (old_line, ''))

            if command == 'SPEAK-CHAR':
                speech['char'] = args
            elif command == 'SPEAK-FILE':
                assert speech is not None
                speech['file'] = args
            elif command == 'SPEAK-ORIG':
                continue
            elif command == 'SPEAK-TEXT':
                assert speech is not None
                speech['text'] = args
                if 'file' in speech:
                    yield '【{char},{file}】あ{text}'.format(**speech)
                else:
                    yield '【{char}】あ{text}'.format(**speech)
                speech = {}
            elif command == 'CODE':
                yield args
            elif command == 'ORIG':
                continue
            elif command == 'TEXT':
                yield 'あ{text}'.format(text=args)

    return '\n'.join(gather_lines()).encode('cp932')
