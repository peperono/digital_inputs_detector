# Convencions de diagrames DrawIO — Arquitectura QP/C++

## Colors

| Color | Hex fill / stroke | Representa |
|---|---|---|
| Blau | `#dae8fc` / `#6c8ebf` | Active Object (`QP::QActive`) i IO strategies (`<<callback>>`) |
| Verd | `#d5e8d4` / `#82b366` | Events QP (entrada o sortida) |
| Groc | `#fff2cc` / `#d6b656` | Memòria compartida entre fils (mutex o g_* globals) |
| Rosa | `#ffcccc` / `#36393d` | Thread extern (Mongoose, etc.) |
| Blanc | `#ffffff` / `#666666` | Actor extern (Browser, client REST) |
| Forma document | — | Fitxer persistent en disc |

## Fletxes

| Estil | Significat |
|---|---|
| Línia contínua | Flux de dades actiu: `read`, `write`, `publish`, `push`, `GET`, `PUT`, `POST` |
| Línia discontínua | Dependència estructural o callback |
| `endArrow=block;endFill=0` | Herència / implementació |
| Fletxa invertida (`startArrow=classic`, `endArrow=none`) | Dades flueixen cap a l'origen |

## Etiquetes de fletxa

| Etiqueta | Mecanisme |
|---|---|
| `publish` | `QF_PUBLISH` — bus QP, tots els subscrits ho reben |
| `POST` | `QActive::POST` — directe a un AO, thread-safe |
| `push` | HttpServer llegeix SharedState i envia per WebSocket |
| `write` | Escriptura a memòria compartida sota mutex |
| `read` | Lectura de memòria compartida sota mutex |

## Estructura d'un Active Object

- Rectangle blau, `verticalAlign=top`, títol en negreta: `Nom (QP::QActive)`
- Sub-rectangles verds a l'interior per a cada event d'entrada i sortida
- Format de l'etiqueta d'event: `NomEvt\n(SIGNAL_NAME, mecanisme)` — dues línies, `fontSize=8`
- Events d'entrada a l'esquerra, events de sortida a la dreta

## IO Strategies (callbacks)

- Rectangle blau amb estereotip `<<abstract>>` per al tipus base (`IOReader`)
- Rectangle blau amb estereotip `<<callback>>` per a cada instància concreta
- Fletxa d'herència (`endArrow=block;endFill=0`) de cada instància cap al tipus base

## Memòria compartida

- Node groc amb els camps llistats dins
- Apareix sempre que hi hagi accés des de més d'un fil o component
- Inclou mutex explícit si és cross-thread; g_* globals si és same-thread (QV cooperatiu)

## Containers (agrupació per fitxer o mòdul)

- Rectangle buit (`fillColor=none`) amb etiqueta de text a l'exterior superior
- Agrupa tots els components definits en el mateix fitxer o carpeta

## Estructura general del diagrama

- Tot dins un `shape=umlFrame` amb títol `NomProjecte / Data`
- Llegenda de colors com a node `text` a la cantonada inferior esquerra
- Fletxes ortogonals (`edgeStyle=orthogonalEdgeStyle`) amb punts de routing explícits en creuaments
