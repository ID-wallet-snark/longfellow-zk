# Benchmarking Guide for mDoc ZK Proofs

Ce guide explique comment exécuter et visualiser les benchmarks de performance pour le système de preuves ZK mDoc.

## 📋 Table des Matières

- [Installation](#installation)
- [Exécution des Benchmarks](#exécution-des-benchmarks)
- [Visualisation des Résultats](#visualisation-des-résultats)
- [Interprétation des Résultats](#interprétation-des-résultats)
- [Options Avancées](#options-avancées)

## 🚀 Installation

### Prérequis

Le système de benchmarking télécharge automatiquement Google Benchmark via CMake. Aucune installation manuelle n'est requise.

Pour la visualisation, installez matplotlib:

```bash
pip3 install matplotlib
```

### Build

```bash
cd /Users/anselme/Documents/projet-ID/longfellow-zk
mkdir -p build && cd build
cmake ..
cmake --build . --target mdoc_benchmark
```

## ▶️ Exécution des Benchmarks

### Méthode 1: Script Automatisé (Recommandé)

Le script `run_benchmarks.sh` exécute les benchmarks et génère automatiquement les visualisations:

```bash
cd lib/circuits/mdoc
./run_benchmarks.sh
```

Ce script:
- ✅ Compile le benchmark si nécessaire
- ✅ Exécute les benchmarks avec répétitions
- ✅ Génère les fichiers JSON et CSV
- ✅ Crée les visualisations automatiquement
- ✅ Sauvegarde les résultats avec timestamp

### Méthode 2: Exécution Manuelle

```bash
# Depuis le répertoire build
./lib/circuits/mdoc/mdoc_benchmark

# Avec options
./lib/circuits/mdoc/mdoc_benchmark --benchmark_repetitions=5
./lib/circuits/mdoc/mdoc_benchmark --benchmark_filter=Prover
```

## 📊 Visualisation des Résultats

### Génération Automatique

Le script `run_benchmarks.sh` génère automatiquement:
- `benchmark_bar_chart.png` - Graphique en barres comparant les temps
- `benchmark_comparison.png` - Comparaison Real Time vs CPU Time

### Génération Manuelle

```bash
# Exporter en JSON
./lib/circuits/mdoc/mdoc_benchmark --benchmark_format=json > results.json

# Générer les visualisations
python3 lib/circuits/mdoc/benchmark_visualizer.py results.json

# Spécifier un répertoire de sortie
python3 lib/circuits/mdoc/benchmark_visualizer.py results.json -o ./charts
```

## 📈 Interprétation des Résultats

### Benchmarks Disponibles

Le fichier `mdoc_benchmark.cc` contient trois benchmarks principaux:

#### 1. **BM_CircuitGeneration**
Mesure le temps de génération du circuit ZK.

**Métriques clés:**
- Real Time: Temps total incluant I/O
- CPU Time: Temps CPU pur
- Iterations: Nombre d'exécutions

**Interprétation:**
- Plus le temps est bas, mieux c'est
- Typiquement quelques millisecondes

#### 2. **BM_Prover**
Mesure le temps d'exécution du prouveur (génération de preuve).

**Métriques clés:**
- Real Time: Temps de génération de preuve
- Iterations: Nombre de preuves générées

**Interprétation:**
- C'est généralement l'opération la plus coûteuse
- Temps attendu: quelques secondes par preuve

#### 3. **BM_Verifier**
Mesure le temps de vérification d'une preuve.

**Métriques clés:**
- Real Time: Temps de vérification
- Iterations: Nombre de vérifications

**Interprétation:**
- La vérification doit être BEAUCOUP plus rapide que la génération
- Temps attendu: quelques millisecondes

### Exemple de Sortie

```
Run on (8 X 2400 MHz CPU s)
CPU Caches:
  L1 Data 32 KiB (x4)
  L1 Instruction 32 KiB (x4)
  L2 Unified 256 KiB (x4)
  L3 Unified 8192 KiB (x1)
----------------------------------------------------------------
Benchmark                      Time             CPU   Iterations
----------------------------------------------------------------
BM_CircuitGeneration        2.45 ms         2.43 ms          287
BM_Prover                   1234 ms         1230 ms            1
BM_Verifier                 3.21 ms         3.19 ms          218
```

## ⚙️ Options Avancées

### Options de Google Benchmark

```bash
# Filtrer les benchmarks
--benchmark_filter=<regex>

# Répétitions multiples
--benchmark_repetitions=<N>

# Format de sortie
--benchmark_format=<console|json|csv>

# Fichier de sortie
--benchmark_out=<filename>

# Afficher les compteurs
--benchmark_counters_tabular=true

# Temps minimum par benchmark
--benchmark_min_time=<seconds>
```

### Exemples d'Utilisation

```bash
# Exécuter uniquement le benchmark du prouveur, 10 fois
./mdoc_benchmark --benchmark_filter=Prover --benchmark_repetitions=10

# Exporter en JSON avec statistiques détaillées
./mdoc_benchmark --benchmark_format=json \
                 --benchmark_repetitions=5 \
                 --benchmark_out=detailed_results.json

# Benchmark rapide (temps minimum réduit)
./mdoc_benchmark --benchmark_min_time=0.1
```

### Personnalisation des Visualisations

Modifiez `benchmark_visualizer.py` pour:
- Changer les couleurs des graphiques
- Ajouter des graphiques supplémentaires
- Modifier les formats d'export (SVG, PDF)
- Ajouter des statistiques personnalisées

## 📁 Organisation des Résultats

```
lib/circuits/mdoc/
├── benchmark_visualizer.py      # Script de visualisation
├── run_benchmarks.sh            # Script d'automatisation
└── benchmark_results/           # Résultats générés
    ├── benchmark_20260107_164500.json
    ├── benchmark_20260107_164500.csv
    ├── charts_20260107_164500/
    │   ├── benchmark_bar_chart.png
    │   └── benchmark_comparison.png
    ├── latest.json              # Symlink vers dernier JSON
    └── latest_charts/           # Symlink vers derniers charts
```

## 🔍 Troubleshooting

### Problème: "benchmark not found"

**Solution:** Le CMakeLists.txt télécharge automatiquement Google Benchmark. Assurez-vous d'avoir une connexion Internet lors du premier build.

### Problème: "matplotlib not found"

**Solution:**
```bash
pip3 install matplotlib
```

### Problème: Benchmarks trop lents

**Solution:** Utilisez `--benchmark_min_time=0.1` pour réduire le temps d'exécution lors du développement.

### Problème: Résultats incohérents

**Solution:** Utilisez `--benchmark_repetitions=10` pour obtenir des statistiques plus fiables.

## 📚 Ressources

- [Google Benchmark Documentation](https://github.com/google/benchmark)
- [Matplotlib Documentation](https://matplotlib.org/stable/contents.html)
- [CMake FetchContent](https://cmake.org/cmake/help/latest/module/FetchContent.html)

## 💡 Bonnes Pratiques

1. **Répétitions:** Utilisez au moins 3-5 répétitions pour des résultats fiables
2. **Isolation:** Fermez les applications lourdes pendant les benchmarks
3. **Cohérence:** Utilisez toujours la même configuration (Release build)
4. **Documentation:** Sauvegardez les résultats avec des notes sur les changements
5. **Comparaison:** Gardez les résultats historiques pour suivre les évolutions
