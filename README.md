# Sistema de Gerenciamento de Rotas (GPS Simplificado)

Aplicativo de terminal escrito em C para cadastrar cidades e estradas, consultar a rede e encontrar a rota de menor distância entre duas cidades.

## Requisitos

- Compilador C com suporte ao padrão usado pelo GCC, como GCC ou MinGW-w64.
- Terminal (PowerShell, Git Bash, Linux ou macOS).

## Compilar e executar

O arquivo-fonte deste repositório se chama `.c`. Abra o terminal na pasta do repositório e compile:

### Windows (PowerShell ou Git Bash)

```bash
gcc .c -o gps_terminal.exe -Wall -Wextra
```

No PowerShell, execute:

```powershell
.\gps_terminal.exe
```

No Git Bash, execute:

```bash
./gps_terminal.exe
```

### Linux ou macOS

```bash
gcc .c -o gps_terminal -Wall -Wextra
./gps_terminal
```

No Windows, também é possível abrir `gps_terminal.exe` pelo Explorador de Arquivos. Executá-lo pelo terminal permite interagir com o menu.

## Como usar

Ao iniciar, o programa procura por `rotas.txt` na pasta atual. Se o arquivo existir, a rede é carregada automaticamente; caso contrário, o sistema começa vazio. Para que essa carga automática funcione, execute o programa a partir da pasta que contém o arquivo.

O menu oferece:

| Opção | Ação |
| --- | --- |
| `1` | Cadastrar uma cidade. Nomes vazios ou que contenham `;` não são aceitos; nomes repetidos não diferenciam maiúsculas de minúsculas. |
| `2` | Cadastrar uma estrada entre duas cidades existentes. Informe cidades pelo nome ou pelo número exibido, uma distância positiva e se a estrada é de mão dupla. A distância aceita ponto ou vírgula decimal. |
| `3` | Listar as cidades e estradas cadastradas. |
| `4` | Calcular a rota de menor distância. Informe origem e destino pelo nome ou número. O resultado mostra o trajeto, a distância total, os trechos e as distâncias acumuladas. |
| `5` | Salvar a rede em um arquivo. Pressione Enter para usar `rotas.txt` ou informe outro nome. |
| `6` | Carregar a rede de um arquivo. Pressione Enter para usar `rotas.txt` ou informe outro nome. O carregamento substitui a rede que está atualmente na memória. |
| `0` | Encerrar o programa. |

O sistema permite até 100 cidades. Estradas são direcionadas: uma estrada de mão dupla é armazenada como duas estradas, uma em cada sentido. Se cadastrar novamente uma estrada existente no mesmo sentido, a distância anterior é atualizada.

## Como o código funciona

### Representação da rede

A rede é modelada como um **grafo**:

- Cada cidade é um vértice, guardado em um vetor de nomes.
- Cada estrada é uma aresta direcionada com uma distância em quilômetros.
- As arestas de cada cidade são armazenadas em uma lista de adjacência encadeada. Assim, a busca de rotas percorre somente as estradas que saem da cidade atual.

As estruturas `RedeDeCidades` e `Estrada` representam, respectivamente, o grafo e cada item da lista de adjacência. A memória das estradas é liberada ao carregar outra rede e ao encerrar o programa.

### Busca da menor rota

O cálculo usa o algoritmo de **Dijkstra**, adequado às distâncias positivas usadas pelo programa:

1. Define a distância da origem como zero e a das demais cidades como infinita.
2. Mantém as cidades a visitar em uma fila de prioridade implementada como heap mínima.
3. Retira a cidade com a menor distância acumulada e examina suas estradas de saída.
4. Atualiza a melhor distância e a cidade anterior sempre que encontra um caminho mais curto.
5. Reconstrói o trajeto a partir das cidades anteriores e exibe a rota e seus custos.

Se não houver caminho na direção solicitada, o programa informa que a rota não existe. Por serem direcionadas, as estradas podem permitir uma viagem de A para B sem permitir o retorno de B para A.

### Persistência dos dados

O programa salva e carrega os dados em texto simples. Cada cidade deve ser registrada antes das estradas que a referenciam. O formato tem dois tipos de linha:

```text
C;NomeDaCidade
E;CidadeDeOrigem;CidadeDeDestino;DistanciaEmQuilometros
```

Exemplo de `rotas.txt`:

```text
C;Fortaleza
C;Caucaia
C;Maracanaú
E;Fortaleza;Caucaia;20.5
E;Caucaia;Fortaleza;20.5
E;Fortaleza;Maracanaú;25
```

As duas linhas entre Fortaleza e Caucaia representam uma estrada de mão dupla. Já a linha de Fortaleza para Maracanaú representa apenas o sentido indicado.

### Utilizar o `rotas.txt` deste repositório

O arquivo `rotas.txt` incluído no projeto já contém cidades e estradas de exemplo. Para usá-lo:

1. Deixe `rotas.txt` na pasta de onde o programa será executado. Ao iniciar, o programa carrega esse arquivo automaticamente se encontrá-lo.
2. Para carregar outro arquivo durante a execução, escolha a opção `6` e informe o nome ou caminho do arquivo. Pressione Enter para carregar `rotas.txt` da pasta atual. Essa operação substitui a rede que está na memória.
3. Para guardar alterações, escolha a opção `5` e pressione Enter para salvar em `rotas.txt`, ou informe outro nome para salvar em um arquivo separado.

Também é possível editar `rotas.txt` em um editor de texto simples. Mantenha as linhas `C` antes das linhas `E`, pois as estradas referenciam cidades já cadastradas. Separe os campos por ponto e vírgula e use ponto como separador decimal nas distâncias. Cada linha `E` representa um sentido; para uma estrada de mão dupla, inclua uma linha para cada direção.

No arquivo de exemplo, as linhas de cidade têm campos extras de coordenadas, por exemplo `C;Acopiara;0.3800;0.5000`. A versão atual do programa usa somente o nome da cidade (o segundo campo); não usa as coordenadas no cálculo de rotas. Ao salvar a rede pela opção `5`, o programa reescreve o arquivo no formato simples `C;NomeDaCidade`, removendo esses campos extras. Faça uma cópia antes de salvar caso queira preservar as coordenadas originais.

Ao salvar, o programa grava primeiro as cidades e depois cada estrada direcionada. O carregador ignora linhas vazias e informa quantas linhas inválidas foram ignoradas. Carregar um arquivo limpa a rede atual antes de preenchê-la.

## Arquivos do repositório

- `.c`: código-fonte do programa.
- `README.md`: instruções de uso e documentação.
- `LICENSE`: licença do projeto.
