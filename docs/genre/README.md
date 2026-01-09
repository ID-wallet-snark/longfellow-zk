# Documentation - Vérification de Genre

Ce dossier contient la documentation complète de la fonctionnalité de vérification de genre par ZK-SNARK.

## Fichiers

- `documentation.typ` - Documentation complète en Typst avec diagrammes
- `documentation.pdf` - Version PDF compilée (générée)

## Compilation

Pour compiler le document Typst en PDF :

```bash
typst compile documentation.typ
```

Ou pour la compilation en mode watch (recompilation automatique) :

```bash
typst watch documentation.typ
```

## Contenu

La documentation couvre :

1. **Introduction** - Contexte et objectifs du projet
2. **Architecture** - Vue d'ensemble du système avec diagrammes
3. **Flux de travail** - Processus de génération et vérification
4. **Implémentation** - Détails techniques (CBOR, circuits ZK)
5. **Interface** - Description de l'interface utilisateur
6. **Tests** - Résultats complets de la suite de tests
7. **Sécurité** - Garanties zero-knowledge
8. **Limitations** - Contraintes actuelles et perspectives

## Diagrammes

Les diagrammes sont créés avec Fletcher (package Typst) et incluent :

- Architecture du système
- Flux de génération de preuve
- Flux de vérification
- Composants logiciels

## Prérequis

Pour compiler la documentation, installer Typst :

```bash
# Via cargo (Rust)
cargo install typst-cli

# Ou télécharger depuis https://github.com/typst/typst/releases
```

## Paquets Typst Utilisés

- `@preview/cetz:0.2.2` - Graphiques et dessins
- `@preview/fletcher:0.5.0` - Diagrammes de flux

Ces paquets seront téléchargés automatiquement lors de la première compilation.
