# Como fazer o dump do inFAMOUS Second Son

O port não traz nenhum arquivo do jogo. Cada pessoa usa o **próprio jogo** (disco ou compra
digital) e tira uma cópia dele de um PS4 desbloqueado. Este guia mostra como.

> Baixar o jogo pronto da internet é pirataria. O port só aceita o dump do seu próprio jogo.

## O que você precisa

| Item | Detalhe |
|---|---|
| Um PS4 (comum, Slim ou Pro) | Com firmware que dê para desbloquear (veja o passo 1) |
| O jogo original | Disco ou compra digital na conta que está no PS4 |
| Um pendrive ou HD externo | Formatado em **exFAT**, com pelo menos **60 GB livres** |
| Um PC com Windows | Para onde a cópia vai |

**Versão recomendada:** a **atualização mais recente** do jogo instalada no PS4
(**01.07**, até onde encontrei). A versão americana é a **CUSA00004**; a de outras regiões
também serve para começar, e a ferramenta de diagnóstico diz qual você tem.

## Passo 1: conferir o firmware

1. No PS4: **Configurações → Sistema → Informações do sistema**. Anote a
   **Versão do software do sistema**.
2. Em 2026, o GoldHEN (o desbloqueio usado pela comunidade) funciona do firmware **4.55 até o
   13.52**. Se o seu PS4 está em uma versão maior que 13.52, ainda não há desbloqueio público.
3. **Não atualize o PS4.** Em **Configurações → Sistema → Downloads automáticos**, desmarque
   **Arquivos de atualização do software do sistema**, e não aceite nenhuma atualização de
   sistema. Uma atualização pode tirar o PS4 da faixa que dá para
   desbloquear, e não tem volta.

## Passo 2: desbloquear com o GoldHEN

O jeito exato depende do firmware (pelo navegador do PS4, por um disco BD-JB ou por cabo de
rede com PPPwn). Siga um guia atualizado para o **seu** firmware, por exemplo:

- [ConsoleMods: primeiros passos no PS4](https://consolemods.org/wiki/PS4:Getting_Started)
- [Guia de instalação do GoldHEN](https://ps4.emiyl.com/installing-goldhen.html)

O desbloqueio do GoldHEN não é permanente: ao desligar o PS4, ele volta ao normal. Para fazer o
dump, basta ativá-lo uma vez.

## Passo 3: deixar o jogo pronto no PS4

1. Instale o jogo: insira o disco ou baixe a compra digital.
2. Abra o jogo uma vez para confirmar que ele inicia. Isso também confirma a licença, no caso
   do digital.
3. Instale a **atualização mais recente** (Opções → Verificar se há atualizações, no ícone
   do jogo).
4. Espere terminar qualquer cópia ou download em segundo plano.

## Passo 4: fazer o dump com o Itemzflow

O Itemzflow é o jeito mais fácil: roda tudo no próprio PS4, sem PC.

1. Instale o Itemzflow no PS4 (é um homebrew; o GoldHEN instala pacotes pelo
   **Package Installer**).
2. Ligue o pendrive ou HD em **exFAT** no PS4.
3. Abra o Itemzflow, selecione **inFAMOUS Second Son**, aperte o botão de opções e escolha
   **Dump** → **Dump all** (jogo base + atualização).
4. O jogo abre por um instante e volta para o Itemzflow; então a cópia começa. Espere a
   barra terminar. Pode levar de 30 minutos a mais de uma hora.

No pendrive vão aparecer duas pastas:

```
CUSA00004-app      ← o jogo base
CUSA00004-patch    ← a atualização
```

Pode apagar o arquivo `.complete`, se aparecer.

**Alternativa (payload do dumper):** se preferir o dumper pelo navegador, use `split=0` no
`dumper.cfg` para juntar jogo e atualização numa pasta só. Detalhes no
[guia do ConsoleMods](https://consolemods.org/wiki/PS4:Ripping_PS4_Game_Backups).

## Passo 5: levar para o PC

1. Ligue o pendrive no PC e copie as pastas para um disco com espaço, por exemplo
   `D:\Jogos\`.
2. Renomeie `CUSA00004-app` para `CUSA00004`. A pasta da atualização pode ficar com o nome
   `CUSA00004-patch`, ao lado dela.
3. Confira que a pasta do jogo tem estes itens:

```
CUSA00004\
  eboot.bin
  sce_module\     (libc.prx e outros módulos)
  sce_sys\        (param.sfo, icon0.png ...)
  ...             (os arquivos de dados do jogo)
```

## Passo 6: rodar o diagnóstico e mandar o resultado

Instale o Python uma vez, se ainda não tiver (no PowerShell):

```powershell
winget install Python.Python.3.13
```

Depois **arraste a pasta `CUSA00004` para cima de `tools\issn\diagnosticar.bat`**. Ou rode:

```powershell
python tools\issn\inspect_dump.py "D:\Jogos\CUSA00004"
```

Para a verificação completa, use o carregador do port do GOW3 que você já tem:

```powershell
python tools\issn\inspect_dump.py "D:\Jogos\CUSA00004" --probe "C:\Users\Joao\Downloads\gow3-windows\bin\gow3-probe.exe"
```

O resultado fica em `out\issn\issn_report.md`. **Esse é o arquivo que eu preciso.** Ele diz se
o dump está bom e lista quais funções do PS4 o jogo usa que o port ainda não tem. Esse é o mapa
do trabalho.

Nenhum arquivo do jogo é alterado pelo diagnóstico, e o relatório não contém nada do jogo, só
nomes de funções e informações da versão.

## Problemas comuns

| Sintoma | Causa e solução |
|---|---|
| Diagnóstico diz "criptografado" | Os arquivos foram copiados direto do PS4 (FTP) em vez de passar pelo dumper. Refaça o passo 4. |
| Diagnóstico diz "CATEGORY=gp" | Você apontou para a pasta da atualização. Aponte para `CUSA00004` (o jogo base). |
| Dumper trava no meio | Espaço insuficiente no pendrive, ou o jogo estava fazendo outra coisa. Libere espaço, deixe o jogo no menu principal e tente de novo. |
| Pendrive não aparece no PS4 | Precisa ser exFAT (ou FAT32), e não NTFS. |
