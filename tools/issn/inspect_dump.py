#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Diagnóstico do dump de inFAMOUS Second Son para o port nativo (issn_pc).

Lê a pasta do jogo (a que tem eboot.bin, sce_module e sce_sys), confere se o dump está
completo e descriptografado, e lista todas as funções do sistema do PS4 que o jogo importa,
dizendo de onde cada uma viria no port:

  runtime   já implementada no runtime em C (src/runtime_*.c), herdado do port do ISSN
  gpu       registrada pelo núcleo do shadPS4 (gpu/), por exemplo GnmDriver e VideoOut
  modulo    exportada por um módulo que vem no próprio dump (sce_module/*.prx)
  FALTA     ninguém implementa: é trabalho a fazer

Nada é alterado na pasta do jogo. Saída: <out>/issn_report.md e <out>/issn_report.json.

Uso:
  python tools/issn/inspect_dump.py "D:\\Jogos\\CUSA00004"
  python tools/issn/inspect_dump.py "D:\\Jogos\\CUSA00004" --probe out\\issn-probe.exe

Com --probe, além da análise estática, prepara a imagem do jogo e roda o carregador real com
--check-imports (a verificação definitiva, a mesma do port do ISSN).
"""
import argparse
import collections
import gzip
import json
import os
from pathlib import Path
import re
import subprocess
import sys

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / 'scripts'))
from prepare import load_sfo, unpack  # noqa: E402
import link_modules  # noqa: E402

# Title IDs conhecidos de inFAMOUS Second Son (PS4). Outros podem funcionar: o relatório avisa.
KNOWN_TITLES = {
    'CUSA00004': 'inFAMOUS Second Son (EUA)',
    'CUSA00223': 'inFAMOUS Second Son (Europa)',
}
# Módulos do dump que o pipeline atual já junta ao jogo (scripts/link_modules.py).
LINKED_BY_DEFAULT = link_modules.DEFAULT_MODULES
SELF_MAGIC = b'O\x15=\x1d'
NID_RE = re.compile(r'"([A-Za-z0-9+-]{11})#([A-Za-z0-9_]+)"')
LIB_FUNCTION_RE = re.compile(r'LIB_FUNCTION\("([A-Za-z0-9+-]{11})",\s*"([A-Za-z0-9_]+)"')


def canonical_library(library):
    """O runtime trata libScePosix como libkernel (src/runtime.c canonical_name)."""
    return 'libkernel' if library == 'libScePosix' else library


def load_names():
    names = {}
    db = Path(__file__).with_name('nid_names.tsv.gz')
    if db.is_file():
        for line in gzip.decompress(db.read_bytes()).decode('utf-8').splitlines():
            nid, _, name = line.partition('\t')
            names[nid] = name
    inc = REPO / 'src' / 'import_names.inc'
    if inc.is_file():
        for nid_lib, name in re.findall(r'\{"([^"]+)","([^"]+)"\}', inc.read_text(encoding='utf-8')):
            names.setdefault(nid_lib.split('#')[0], name)
    return names


def implemented_sets():
    """(runtime, runtime_nids, gpu): 'NID#biblioteca' que o código do port resolve, e os NIDs das
    funções que o runtime exporta pelo nome (qualquer biblioteca: src/runtime_export_nids.inc)."""
    runtime, runtime_nids, gpu = set(), set(), set()
    for path in (REPO / 'src').glob('*.c'):
        for nid, library in NID_RE.findall(path.read_text(encoding='utf-8', errors='replace')):
            runtime.add(f'{nid}#{canonical_library(library)}')
    # Exported by name and found through import_names.inc (runtime_symbol reads both tables).
    sys.path.insert(0, str(REPO / 'tools' / 'issn'))
    import gen_export_nids
    exported = set(gen_export_nids.exported_names())
    names_inc = REPO / 'src' / 'import_names.inc'
    if names_inc.is_file():
        for scoped, symbol in re.findall(r'\{"([^"#]+#[^"]+)","([^"]+)"\}', names_inc.read_text(encoding='utf-8')):
            if symbol in exported:
                code, library = scoped.split('#', 1)
                runtime.add(f'{code}#{canonical_library(library)}')
    exports = REPO / 'src' / 'runtime_export_nids.inc'
    if exports.is_file():
        runtime.update(f'{code}#{canonical_library(library)}' for code, library in
                       re.findall(r'\{"([A-Za-z0-9+-]{11})#([A-Za-z0-9_]+)","', exports.read_text(encoding='utf-8')))
    for folder in (REPO / 'gpu' / 'shadps4', REPO / 'gpu' / 'shim'):
        for path in folder.rglob('*.cpp'):
            text = path.read_text(encoding='utf-8', errors='replace')
            for nid, library in LIB_FUNCTION_RE.findall(text):
                gpu.add(f'{nid}#{canonical_library(library)}')
            for nid, library in NID_RE.findall(text):
                gpu.add(f'{nid}#{canonical_library(library)}')
    return runtime, runtime_nids, gpu


def file_kind(path):
    """'elf', 'self', 'self-encrypted' ou uma descrição do problema."""
    try:
        with path.open('rb') as stream:
            head = stream.read(4096)
    except OSError as error:
        return f'ilegível ({error})'
    if not head:
        return 'vazio (0 bytes)'
    if head[:4] == b'\x7fELF':
        return 'elf'
    if head[:4] != SELF_MAGIC:
        return 'formato desconhecido'
    count, = unpack('<H', head, 24)
    for i in range(min(count, 120)):
        if 32 + i * 32 + 32 > len(head):
            break
        flags, = unpack('<Q', head, 32 + i * 32)
        if flags & 0x800 and flags & 10:
            return 'self-encrypted'
    return 'self'


def imports_and_exports(path):
    """(imports, exports) de um eboot/prx: cada item é (nid, biblioteca, é_dado)."""
    m = link_modules.module(path)
    imports, exports = [], []
    for symbol in m['symbols']:
        identity = symbol['identity']
        if not identity:
            continue
        nid, (library, _), _ = identity
        item = (nid, canonical_library(library), symbol['type'] == 1)
        (exports if symbol['section'] else imports).append(item)
    needed_modules = sorted({name for name, _ in m['modules'].values()})
    return imports, exports, needed_modules


def find_updates(game, title_id):
    found = []
    for suffix in ('-patch', '-UPDATE', '-update', '-Patch'):
        candidate = game.parent / f'{title_id}{suffix}'
        if candidate.is_dir():
            found.append(candidate)
    return found


def run_probe(probe, game, out, modules):
    """Prepara a imagem com os scripts do port e roda o carregador com --check-imports."""
    out.mkdir(parents=True, exist_ok=True)
    python = sys.executable
    env = {**os.environ, 'PYTHONIOENCODING': 'utf-8'}  # the scripts print game titles
    steps = [
        [python, str(REPO / 'scripts' / 'prepare.py'), str(game), '--out', str(out)],
        [python, str(REPO / 'scripts' / 'link_modules.py'), str(game), '--out', str(out), '--modules', *modules],
        [str(probe), str(out / 'boot-linked.bin'), '--app0', str(game), '--check-imports'],
    ]
    log = []
    for command in steps:
        result = subprocess.run(command, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, text=True, encoding='utf-8', errors='replace',
                                timeout=600, env=env)
        log.append(f'$ {" ".join(command)}\n{result.stdout}(saída {result.returncode})')
        if result.returncode and command is not steps[-1]:
            break
    return '\n\n'.join(log)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('game', type=Path, help='pasta do jogo (com eboot.bin, sce_module e sce_sys)')
    parser.add_argument('--out', type=Path, default=REPO / 'out' / 'issn', help='onde gravar o relatório')
    parser.add_argument('--probe', type=Path, help='issn-probe.exe para a verificação definitiva')
    args = parser.parse_args()
    # The report is UTF-8 (Portuguese, the ✅/❌ marks): a pipe or file gets UTF-8 whatever the
    # code page (Windows Pythons write ANSI to pipes); a console shows it as is.
    if sys.stdout.isatty():
        sys.stdout.reconfigure(errors='replace')
    else:
        sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    game, out = args.game, args.out
    out.mkdir(parents=True, exist_ok=True)
    problems, notes = [], []
    report = {'game_dir': str(game.resolve())}

    # 1. Estrutura do dump
    for required in ('eboot.bin', 'sce_sys/param.sfo', 'sce_module'):
        if not (game / required).exists():
            problems.append(f'Falta `{required}` na pasta do jogo.')
    if problems:
        return finish(out, report, problems, notes, None)

    sfo = {}
    try:
        sfo = load_sfo(game / 'sce_sys' / 'param.sfo')
    except (OSError, ValueError) as error:
        problems.append(f'param.sfo ilegível: {error}')
    title_id = str(sfo.get('TITLE_ID', '?'))
    report['sfo'] = {k: sfo.get(k) for k in ('TITLE', 'TITLE_ID', 'APP_VER', 'VERSION', 'CATEGORY', 'CONTENT_ID')}
    if title_id not in KNOWN_TITLES:
        notes.append(f'Title ID {title_id} ainda não está na lista de versões conhecidas de Second Son '
                     f'({", ".join(KNOWN_TITLES)}); a análise continua normalmente.')
    if sfo.get('CATEGORY') == 'gp':
        problems.append('Esta pasta é só a atualização (CATEGORY=gp). Aponte para o jogo base, '
                        'ou para a pasta com base e atualização juntas.')
    updates = find_updates(game, title_id)
    report['update_folders'] = [str(p) for p in updates]
    if updates:
        notes.append('Encontrei pasta(s) de atualização ao lado do jogo: '
                     + ', '.join(p.name for p in updates)
                     + '. O launcher do port junta as duas (Game → Update folder).')

    kinds = {'eboot.bin': file_kind(game / 'eboot.bin')}
    modules_dir = game / 'sce_module'
    bundled = sorted(p.name for p in modules_dir.iterdir() if p.is_file()) if modules_dir.is_dir() else []
    for name in bundled:
        kinds[f'sce_module/{name}'] = file_kind(modules_dir / name)
    report['files'] = kinds
    for name, kind in kinds.items():
        if kind == 'self-encrypted':
            problems.append(f'`{name}` está criptografado: foi copiado direto do PS4 em vez de passar '
                            'por um dumper. Refaça o dump (veja docs/issn/DUMP.md).')
        elif kind not in ('elf', 'self'):
            problems.append(f'`{name}`: {kind}.')
    for module in LINKED_BY_DEFAULT:
        if module not in bundled:
            notes.append(f'O dump não traz `sce_module/{module}`, que o pipeline do ISSN junta ao jogo; '
                         'o link será feito só com os módulos presentes.')
    data_bytes = sum(p.stat().st_size for p in game.rglob('*') if p.is_file())
    report['dump_bytes'] = data_bytes
    if kinds['eboot.bin'] not in ('elf', 'self'):
        return finish(out, report, problems, notes, None)

    # 2. Imports do jogo e de onde cada um viria
    names = load_names()
    runtime, runtime_nids, gpu = implemented_sets()
    eboot_imports, _, needed_modules = imports_and_exports(game / 'eboot.bin')
    report['needed_modules'] = needed_modules
    module_exports = {}
    module_imports = {}
    for name in bundled:
        if kinds[f'sce_module/{name}'] not in ('elf', 'self'):
            continue
        try:
            imports, exports, _ = imports_and_exports(modules_dir / name)
        except (ValueError, KeyError, StopIteration, IndexError) as error:
            notes.append(f'Não consegui ler `{name}`: {error}')
            continue
        module_imports[name] = imports
        for nid, library, _ in exports:
            module_exports.setdefault(f'{nid}#{library}', name)

    linked = [m for m in LINKED_BY_DEFAULT if m in bundled]
    wanted = [('eboot.bin', eboot_imports)] + [(m, module_imports.get(m, [])) for m in linked]
    rows, seen = [], set()
    for origin, imports in wanted:
        for nid, library, is_data in imports:
            key = f'{nid}#{library}'
            if key in seen:
                continue
            seen.add(key)
            if key in runtime or nid in runtime_nids:
                source = 'runtime'
            elif key in gpu:
                source = 'gpu'
            elif key in module_exports or (library == 'libSceLibcInternal' and f'{nid}#libc' in module_exports):
                # link_modules.py serves libSceLibcInternal (not shipped) with libc.prx's same-NID exports.
                owner = module_exports.get(key) or module_exports[f'{nid}#libc']
                source = 'modulo' if owner in linked else f'modulo:{owner}'
            else:
                source = 'FALTA'
            rows.append(dict(nid=nid, library=library, name=names.get(nid, '?'), data=is_data,
                             source=source, importer=origin))
    report['imports'] = rows
    by_library = collections.defaultdict(collections.Counter)
    for row in rows:
        by_library[row['library']][row['source'].split(':')[0]] += 1
    report['by_library'] = {lib: dict(c) for lib, c in sorted(by_library.items())}
    unlinked = sorted({r['source'].split(':', 1)[1] for r in rows if r['source'].startswith('modulo:')})
    if unlinked:
        notes.append('Funções que existem em módulos do dump que o pipeline ainda não junta ao jogo: '
                     + ', '.join(unlinked) + '. Dá pra adicioná-los em link_modules.py (--modules).')

    probe_log = None
    if args.probe:
        if not args.probe.is_file():
            problems.append(f'--probe: {args.probe} não existe.')
        else:
            probe_log = run_probe(args.probe, game, out / 'probe', linked)
    return finish(out, report, problems, notes, probe_log)


def finish(out, report, problems, notes, probe_log):
    report['problems'], report['notes'] = problems, notes
    if probe_log:
        report['probe_log'] = probe_log
    (out / 'issn_report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    md = render(report)
    (out / 'issn_report.md').write_text(md, encoding='utf-8')
    print(md)
    print(f'\nRelatório salvo em: {out / "issn_report.md"}')
    return 1 if problems else 0


def render(report):
    lines = ['# Diagnóstico do dump: inFAMOUS Second Son', '']
    sfo = report.get('sfo') or {}
    if sfo:
        lines += [f"- Jogo: **{sfo.get('TITLE')}** · {sfo.get('TITLE_ID')} · versão {sfo.get('APP_VER')} "
                  f"(categoria {sfo.get('CATEGORY')})"]
    lines += [f"- Pasta: `{report['game_dir']}`"]
    if 'dump_bytes' in report:
        lines += [f"- Tamanho do dump: {report['dump_bytes'] / 2**30:.1f} GiB"]
    if report.get('files'):
        lines += ['', '## Arquivos executáveis', '']
        lines += [f'- `{name}`: {kind}' for name, kind in report['files'].items()]
    lines += ['', '## Problemas', '']
    lines += [f'- ❌ {p}' for p in report['problems']] or ['- Nenhum. O dump está utilizável. ✅']
    if report['notes']:
        lines += ['', '## Observações', ''] + [f'- {n}' for n in report['notes']]
    rows = report.get('imports')
    if rows:
        total = collections.Counter(r['source'].split(':')[0] for r in rows)
        lines += ['', '## Funções do sistema que o jogo usa', '',
                  f"Total: **{len(rows)}** · já no runtime: {total['runtime']} · núcleo do shadPS4: {total['gpu']} "
                  f"· módulos do dump: {total['modulo']} · **faltando: {total['FALTA']}**", '',
                  '| Biblioteca | runtime | gpu | módulo | FALTA |', '|---|---:|---:|---:|---:|']
        for lib, c in sorted(report['by_library'].items(), key=lambda kv: -kv[1].get('FALTA', 0)):
            lines.append(f"| {lib} | {c.get('runtime', 0)} | {c.get('gpu', 0)} | {c.get('modulo', 0)} | "
                         f"{c.get('FALTA', 0)} |")
        missing = [r for r in rows if r['source'] == 'FALTA']
        if missing:
            lines += ['', '### Faltando (o trabalho do port)']
            current = None
            for r in sorted(missing, key=lambda r: (r['library'], r['name'])):
                if r['library'] != current:
                    current = r['library']
                    lines += ['', f'**{current}**', '']
                lines.append(f"- `{r['name']}` ({r['nid']}){' · dado' if r['data'] else ''}")
    if report.get('probe_log'):
        lines += ['', '## Verificação com o carregador real (--check-imports)', '', '```',
                  report['probe_log'][-20000:], '```']
    lines += ['', '_Análise estática: a contagem é uma estimativa. A verificação definitiva é a do '
              'carregador (--probe)._']
    return '\n'.join(lines) + '\n'


if __name__ == '__main__':
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(130)
