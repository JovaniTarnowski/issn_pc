# issn_pc: roadmap até a Alpha

Port nativo de **inFAMOUS Second Son** (PS4) para Windows, construído sobre a base do
[gow3_pc](https://github.com/FelipePrado0/gow3_pc) (que veio do bloodborne_pc).

| | |
|---|---|
| Base | `gow3_pc`, branch `main` (commit `11219a1`, 2026-10-14), histórico preservado |
| Branch de trabalho | `issn` |
| Jogo alvo | inFAMOUS Second Son, **CUSA00004** (EUA), versão **01.00** (a versão dos patches e testes do fork do Ayoub) |
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
| 1 | Dump do jogo e diagnóstico (`tools/issn/inspect_dump.py`) | ✅ feito: CUSA00004 01.00 (digital EU, `EP9000`), 497 funções, 0 faltando | dump |
| 2 | Ambiente de build no Windows (MSYS2 CLANG64) e build do `issn-probe` | ✅ feito (RX 9060 XT, Vulkan OK) | |
| 3 | Primeiro boot: implementar as funções que faltam, uma a uma, até a primeira tela | 🟡 o código do jogo roda até ler `art/` (threads, memória de vídeo, troféus) | 1, 2 |
| 4 | Gráficos: trazer as correções do fork do Ayoub para `gpu/shadps4` | 🟡 correções de renderização feitas; desempenho depois | 3 |
| 5 | Áudio, vídeos (Bink/AvPlayer), controle e saves | a fazer | 3 |
| 6 | Prólogo jogável (critérios da Alpha) | a fazer | 4, 5 |
| 7 | Identidade: renomear `issn` → `issn`, launcher com a cara do Second Son | a fazer | 6 |
| 8 | Patches (60 FPS, resolução) e menu no jogo (cheats de poder e karma) | depois da Alpha | 6 |

### Etapa 1: o que o diagnóstico responde

- O dump está completo e descriptografado?
- Qual versão e região?
- Quantas funções do sistema o jogo usa e quantas já existem no port (runtime do ISSN +
  núcleo do shadPS4 + módulos que vêm no dump)?
- **Lista das que faltam, por biblioteca.** É o mapa da etapa 3.

### Etapa 3: método (o mesmo do ISSN)

1. Rodar `issn-probe --check-imports` e implementar o que estiver faltando, começando pelas
   bibliotecas que o jogo chama primeiro na inicialização (kernel, sysmodule, userservice).
2. Rodar o jogo. A cada `STOP:` ou `Fault:` no `user/last_run.log`, implementar ou corrigir.
3. Funções de serviços online (NP, trophies, PSN) entram como versões "vazias" que respondem
   "offline" ou "ok", como o ISSN fez em `runtime_services.c`.

### Etapa 3: o que o primeiro boot ensinou

O executável do Second Son (SDK 1.50, 2014) é **ET_SCE_EXEC**: ligado em 0x400000, enquanto o do
God of War III Remastered é relocável (ET_SCE_DYNEXEC). Diferenças que travavam e já foram
resolvidas:

| Problema | Correção |
|---|---|
| `libc.prx` antiga: relocação DTPMOD64 de um símbolo TLS que ela mesma exporta | `link_modules.py` aceita (é o bloco TLS do próprio módulo) |
| `link_libc.py` (linker antigo, saída não usada) rejeitava símbolos de nome comum | `run.py` não o chama mais |
| O ponteiro de thread (`mov r64, fs:[0]`) lido com 10 registradores, não só `rax` | reescrita das 16 formas (3.392 lugares) no Linux e no Windows |
| Funções que o runtime implementa pelo nome só eram achadas se o ISSN as importasse | `src/runtime_export_nids.inc` (gerado): NID#biblioteca de cada nome exportado |
| 17 funções sem implementação | event flags do kernel (de verdade, com teste de threads), PlayGo, HTTP/NetCtl offline, NP, DLC, SystemService |
| Endereços de patch de um executável em 0x400000 | `patches.py` usa base 0 nesse caso; patch fora do executável é pulado, não fatal |

Até onde chega (Linux, GPU por software, só com o executável): `libc` e `Fios2` iniciam, o jogo
cria as threads de arquivo, as 5 `JobWorker`, a de troféus, aloca 4 GB de memória de vídeo e lê
`art/`. Próximo passo: rodar com o dump inteiro no Windows.

Os patches do fork do Ayoub são de **outro build** da 01.00 (o dump é o digital europeu): todas
as linhas são puladas pelo `OriginalHash`. Para usá-los neste build, é preciso achar os mesmos
pontos no código dele (assinaturas).

### Etapa 4: o que já entrou (gráficos)

O fork do Ayoub é **um commit** sobre o shadPS4 `2338a06` (30/09/2026), com ~14.700 linhas em 92
arquivos, quase tudo desempenho. O shadPS4 do port está na base `b4e7ae7`, com as mudanças do ISSN.
Um merge de três vias completo dá 195 conflitos (os dois otimizaram as mesmas peças). Por isso
entrou primeiro só o que muda a **imagem**:

| Correção | Origem | Para quê |
|---|---|---|
| Multiplicação "legacy" em `V_MAC/V_MAD_LEGACY_F32` | Ayoub | 0 × ∞ = 0 como no PS4; sem isso, NaN (pixels pretos ou estourados) |
| Atomics em imagens 1D | Ayoub | shaders de compute que contam em texturas 1D |
| Barreiras de memória entre "waves" (`ISSN_WAVE_MEMORY_BARRIERS`) | Ayoub | compute shaders que contam com 64 threads em sincronia; ligado só para o Second Son (`scripts/game_profiles.py`) |
| S# com campos inválidos → sampler padrão | shadPS4 `1a0f5be8` | evita sampler Vulkan inválido |
| 7 correções de shader do shadPS4 oficial | shadPS4 | ver `gpu/VENDOR.txt` |
| Patches do Ayoub com `OriginalHash` | Ayoub | qualidade dinâmica ×2, trava anti-congelamento, lista de luzes maior; resolução e FOV opcionais |

Fica para a fase de desempenho (com o jogo para medir): gravação de comandos em thread própria,
fila de readback, pipelines assíncronos, occlusion queries reais, e os reworks grandes do
shadPS4 oficial. O modo PS4 Pro (e o patch do checkerboard) também fica para depois: o port roda
o jogo no modo PS4 normal.

### Controle (DualShock 4 / DualSense)

O Second Son usa mais o controle do PS4 que o God of War III. Feito, sem depender do jogo
(`src/runtime_pad.c`, testes `tests/test_pad.c` e `tests/test_pad_map.c` com um controle
virtual do SDL):

| Recurso | Como fica no PC |
|---|---|
| Touchpad (dedos e clique) | já vinha do port do ISSN: dedos reais do DS4/DualSense |
| Sensores de movimento | acelerômetro (em G) e giroscópio (rad/s) do controle; orientação integrada do giroscópio; `scePadSetMotionSensorState` e `scePadResetOrientation` funcionam |
| Chacoalhar (lata de spray) | tecla **G**, ou Share (Xbox Series) / Mute (DualSense); mapeável em Controles |
| Light bar | a cor que o jogo pede vai para o controle (karma), só quando muda; `ISSN_LIGHTBAR=0` desliga |

A confirmar com o jogo: a unidade da aceleração (G, como no SDK; `ISSN_PAD_ACCEL_UNITS=ms2`
passa m/s² como o shadPS4) e o eixo do chacoalhar que o jogo espera. Ainda não: alto-falante do
controle (áudio no port PAD_SPK) e inclinar com o teclado.

### Riscos conhecidos

| Risco | Por que | Plano |
|---|---|---|
| Mundo aberto com streaming | O Second Son lê dados do disco o tempo todo; o ISSN era mais linear. | Atenção a `runtime_file.c` e a I/O assíncrona. |
| Mais bibliotecas do sistema | O jogo usa recursos do PS4 que o ISSN não usava (touchpad, light bar, share, câmera). | O diagnóstico mede. Versões vazias primeiro. |
| Gráficos | A versão principal do shadPS4 tem glitches e crashes no Second Son. | Portar as correções do fork do Ayoub, que já roda o jogo inteiro. |
| CPU | O fork do Ayoub é descrito como pesado de CPU. | Medir; o caminho nativo deve ajudar. |

## Pendências do usuário

- [ ] Dump do jogo (guia: [DUMP.md](DUMP.md)).
- [ ] Rodar o diagnóstico e enviar `out/issn/issn_report.md`.
- [x] Instalar o MSYS2 no PC (para a etapa 2).

## Referências

- [gow3_pc](https://github.com/FelipePrado0/gow3_pc): a base deste projeto.
- [shadPS4](https://github.com/shadps4-emu/shadPS4): núcleo de vídeo; também a fonte dos nomes
  das funções (`tools/issn/nid_names.tsv.gz`, gerado do `aerolib.inl`, GPL-2.0-or-later).
- [Fork do Ayoub para o Second Son](https://github.com/weshayoub92/shadPS4-perf/releases/tag/v1.0)
- [Relatório de compatibilidade CUSA00004](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1333)
