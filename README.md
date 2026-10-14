# issn_pc: inFAMOUS Second Son nativo no PC

Port nativo de **inFAMOUS Second Son** (PS4) para Windows, **em desenvolvimento**. Ainda não roda.

Construído sobre o [gow3_pc](https://github.com/FelipePrado0/gow3_pc), o port do God of War III
Remastered, que vem do [bloodborne_pc](https://github.com/Supermedo/bloodborne_pc). O código do
jogo roda direto no processador (o PS4 é x86-64), um runtime em C substitui o sistema do PS4 e os
gráficos são traduzidos para Vulkan pelo núcleo do [shadPS4](https://github.com/shadps4-emu/shadPS4).

> **Nenhum arquivo do jogo vem neste projeto.** Você precisa do dump do seu próprio jogo:
> veja [docs/issn/DUMP.md](docs/issn/DUMP.md).
> Projeto sem relação com a Sony Interactive Entertainment ou a Sucker Punch Productions.
> inFAMOUS é marca da Sony Interactive Entertainment.

## Onde estamos

Veja o [roadmap](docs/issn/ROADMAP.md). Agora: **etapa 1, diagnóstico do dump.**

```powershell
python tools\issn\inspect_dump.py "D:\Jogos\CUSA00004"
```

## Base herdada

A documentação original do port do God of War III (build, launcher, testes, créditos) está em
[README.gow3.md](README.gow3.md). Créditos: deadinside28 (bloodborne_pc), Supermedo (Windows),
Felipe Prado (gow3_pc), cuesta4 e a equipe do shadPS4.

## Licença

GNU GPL v2 ou posterior ([LICENSE](LICENSE)). Componentes de terceiros mantêm suas licenças.
