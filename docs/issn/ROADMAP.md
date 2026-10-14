# issn_pc: roadmap até a Alpha

Port nativo de **inFAMOUS Second Son** (PS4) para Windows, construído sobre a base do
[gow3_pc](https://github.com/FelipePrado0/gow3_pc) (que veio do bloodborne_pc).

| | |
|---|---|
| Base | `gow3_pc`, branch `main` (commit `11219a1`, 2026-10-14), histórico preservado |
| Branch de trabalho | `issn` |
| Jogo alvo | inFAMOUS Second Son, **CUSA00004** (EUA), versão **01.07** (a confirmar com o dump) |
| Plataforma | Windows 10/11 64-bit, Vulkan 1.3 |
| Referência gráfica | Fork do shadPS4 do Ayoub para o Second Son (60 FPS, 4K, sem os glitches da versão principal) |

## Como funciona (versão curta)

O PS4 tem processador x86-64, igual ao PC. O `eboot.bin` do jogo roda **sem tradução**,
direto no processador. O port troca o "sistema operacional" do PS4 por um feito em C
(`src/runtime_*.c`) e traduz os gráficos para Vulkan com o núcleo do shadPS4 (`gpu/`). Quando o
jogo chama uma função do sistema que o port não tem, o carregador para e diz o nome:
`STOP: first unsupported PS4 import: <nome>`. O trabalho é implementar essas funções e
corrigir os gráficos.

## Definição da Alpha

O jogo abre pelo carregador nativo, toca a abertura e roda o prólogo (Delsin na reserva Akomish
até a primeira luta com poder de fumaça), com som, vídeo e controle, e salva.

- [ ] Chega ao menu principal e inicia um New Game.
- [ ] O prólogo roda sem travar até o primeiro save.
- [ ] Vídeos, música, vozes e efeitos funcionam.
- [ ] Controle (SDL3) e teclado funcionam.
- [ ] O save grava e carrega depois de reiniciar.
- [ ] O log não tem `STOP:` nem `Fault:` no percurso.

## Etapas

| # | Etapa | Status | Depende de |
|---|---|---|---|
| 0 | Estrutura do projeto, ferramenta de diagnóstico, guia de dump | ✅ feito | |
| 1 | Dump do jogo e diagnóstico (`tools/issn/inspect_dump.py`) | ⏳ aguardando dump | dump |
| 2 | Ambiente de build no Windows (MSYS2 CLANG64) e build do `gow3-probe` | a fazer | |
| 3 | Primeiro boot: implementar as funções que faltam, uma a uma, até a primeira tela | a fazer | 1, 2 |
| 4 | Gráficos: trazer as correções do fork do Ayoub para `gpu/shadps4` | a fazer | 3 |
| 5 | Áudio, vídeos (Bink/AvPlayer), controle e saves | a fazer | 3 |
| 6 | Prólogo jogável (critérios da Alpha) | a fazer | 4, 5 |
| 7 | Identidade: renomear `gow3` → `issn`, launcher com a cara do Second Son | a fazer | 6 |
| 8 | Patches (60 FPS, resolução) e menu no jogo (cheats de poder e karma) | depois da Alpha | 6 |

### Etapa 1: o que o diagnóstico responde

- O dump está completo e descriptografado?
- Qual versão e região?
- Quantas funções do sistema o jogo usa e quantas já existem no port (runtime do GOW3 +
  núcleo do shadPS4 + módulos que vêm no dump)?
- **Lista das que faltam, por biblioteca.** É o mapa da etapa 3.

### Etapa 3: método (o mesmo do GOW3)

1. Rodar `gow3-probe --check-imports` e implementar o que estiver faltando, começando pelas
   bibliotecas que o jogo chama primeiro na inicialização (kernel, sysmodule, userservice).
2. Rodar o jogo. A cada `STOP:` ou `Fault:` no `user/last_run.log`, implementar ou corrigir.
3. Funções de serviços online (NP, trophies, PSN) entram como versões "vazias" que respondem
   "offline" ou "ok", como o GOW3 fez em `runtime_services.c`.

### Riscos conhecidos

| Risco | Por que | Plano |
|---|---|---|
| Mundo aberto com streaming | O Second Son lê dados do disco o tempo todo; o GOW3 era mais linear. | Atenção a `runtime_file.c` e a I/O assíncrona. |
| Mais bibliotecas do sistema | O jogo usa recursos do PS4 que o GOW3 não usava (touchpad, light bar, share, câmera). | O diagnóstico mede. Versões vazias primeiro. |
| Gráficos | A versão principal do shadPS4 tem glitches e crashes no Second Son. | Portar as correções do fork do Ayoub, que já roda o jogo inteiro. |
| CPU | O fork do Ayoub é descrito como pesado de CPU. | Medir; o caminho nativo deve ajudar. |

## Pendências do usuário

- [ ] Dump do jogo (guia: [DUMP.md](DUMP.md)).
- [ ] Rodar o diagnóstico e enviar `out/issn/issn_report.md`.
- [ ] Instalar o MSYS2 no PC (para a etapa 2).

## Referências

- [gow3_pc](https://github.com/FelipePrado0/gow3_pc): a base deste projeto.
- [shadPS4](https://github.com/shadps4-emu/shadPS4): núcleo de vídeo; também a fonte dos nomes
  das funções (`tools/issn/nid_names.tsv.gz`, gerado do `aerolib.inl`, GPL-2.0-or-later).
- [Fork do Ayoub para o Second Son](https://github.com/weshayoub92/shadPS4-perf/releases/tag/v1.0)
- [Relatório de compatibilidade CUSA00004](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1333)
