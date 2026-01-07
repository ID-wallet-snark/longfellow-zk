# Analyse de Scalabilité - mDoc ZK Proofs

## 🎯 Objectif

Déterminer la complexité algorithmique du système de preuves ZK mDoc en mesurant les performances pour 1, 2, 3, et 4 attributs.

## 📊 Métriques Mesurées

### Temps d'Exécution
- **Circuit Generation**: Temps pour générer le circuit ZK
- **Proof Generation (Prover)**: Temps pour créer la preuve
- **Verification**: Temps pour vérifier la preuve
- **Total Time**: Somme des trois phases

### Tailles
- **Circuit Size**: Taille du circuit compilé (bytes)
- **Proof Size**: Taille de la preuve ZK (bytes)
- **Proof Bytes per Attribute**: Efficacité (proof_size / num_attributes)

### Ratios
- **Prover/Verifier Ratio**: Ratio des temps prover/verifier
  - Plus élevé = meilleur pour ZK (vérification rapide)

## 🔬 Modèles de Régression

Pour chaque métrique, nous testons 3 modèles:

### 1. Linéaire
```
Time(n) = a·n + b
```
- **Interprétation**: Complexité O(n)
- **Idéal pour**: Systèmes bien optimisés

### 2. Quadratique
```
Time(n) = a·n² + b·n + c
```
- **Interprétation**: Complexité O(n²)
- **Courant pour**: Algorithmes avec boucles imbriquées

### 3. Exponentielle
```
Time(n) = a·e^(b·n)
```
- **Interprétation**: Complexité O(2^n)
- **Problématique**: Non scalable

## 📈 Coefficient de Détermination (R²)

Le R² mesure la qualité de l'ajustement:
- **R² = 1.0**: Ajustement parfait
- **R² > 0.95**: Excellent ajustement
- **R² > 0.90**: Bon ajustement
- **R² < 0.90**: Ajustement médiocre

Le modèle avec le **R² le plus élevé** indique la complexité réelle.

## 🚀 Utilisation

### 1. Exécuter le Benchmark

```bash
cd build
./lib/circuits/mdoc/multi_attribute_benchmark --benchmark_format=json > scalability_results.json 2>/dev/null
```

**Durée estimée**: ~5-10 minutes (4 configurations × ~1-2 min chacune)

### 2. Analyser les Résultats

```bash
cd lib/circuits/mdoc
source venv/bin/activate
pip install scipy  # Si pas déjà installé
python3 scalability_analyzer.py ../../../build/scalability_results.json -o ./analysis
```

### 3. Résultats Générés

Le script crée dans `./analysis/`:

#### Graphiques PNG (300 DPI, publication-ready)
- **`time_vs_attributes.png`**: 4 sous-graphiques avec courbes de régression
  - Circuit Generation Time
  - Proof Generation Time  
  - Verification Time
  - Total Time
- **`size_vs_attributes.png`**: Tailles vs nombre d'attributs
  - Proof Size
  - Circuit Size
- **`performance_ratios.png`**: Ratios de performance
  - Prover/Verifier Ratio
  - Proof Bytes per Attribute

#### Tableau LaTeX
- **`scalability_table.tex`**: Tableau formaté pour publication

#### Analyse Console
- Équations de régression pour chaque métrique
- R² pour chaque modèle
- Identification du meilleur modèle

## 📊 Exemple de Résultats

```
COMPLEXITY ANALYSIS
======================================================================

Circuit Generation:
  Linear: R² = 0.9234
    Equation: y = 1.23e+01x + 4.56e+01
  Quadratic: R² = 0.9876
    Equation: y = 2.34e+00x² + 5.67e+00x + 3.45e+01
  Exponential: R² = 0.9543
    Equation: y = 3.45e+01 * e^(1.23e-01x)
  ✓ Best fit: QUADRATIC (R² = 0.9876)

Proof Generation (Prover):
  Linear: R² = 0.9987
    Equation: y = 1.23e+00x + 1.23e+01
  Quadratic: R² = 0.9989
    Equation: y = 1.23e-02x² + 1.15e+00x + 1.20e+01
  Exponential: R² = 0.9765
    Equation: y = 1.23e+01 * e^(5.67e-02x)
  ✓ Best fit: QUADRATIC (R² = 0.9989)
```

## 🎓 Interprétation pour Publication

### Si Linéaire (R² > 0.95)
> "Le système présente une complexité **linéaire O(n)** en fonction du nombre d'attributs, démontrant une excellente scalabilité."

### Si Quadratique (R² > 0.95)
> "Le système présente une complexité **quadratique O(n²)**, typique des systèmes de preuves ZK. Cela reste acceptable pour un nombre modéré d'attributs (< 10)."

### Si Exponentielle (R² > 0.95)
> "Le système présente une complexité **exponentielle O(2^n)**, limitant la scalabilité. Optimisations recommandées pour supporter plus de 4-5 attributs."

## 📝 Tableau Comparatif Type

| #Attrs | Circuit (MB) | Proof (MB) | Circuit Gen (s) | Prover (s) | Verifier (s) | P/V Ratio |
|--------|--------------|------------|-----------------|------------|--------------|-----------|
| 1      | 83.7         | 5.2        | 48.5            | 12.5       | 6.3          | 2.0       |
| 2      | 167.3        | 10.4       | 97.2            | 25.1       | 12.6         | 2.0       |
| 3      | 251.0        | 15.6       | 145.8           | 37.6       | 18.9         | 2.0       |
| 4      | 334.6        | 20.8       | 194.5           | 50.2       | 25.2         | 2.0       |

## 🔍 Points Clés à Analyser

1. **Scalabilité du Prover**: 
   - Linéaire = Excellent
   - Quadratique = Acceptable
   - Exponentielle = Problématique

2. **Efficacité de la Preuve**:
   - `Proof Bytes per Attribute` devrait rester constant ou diminuer

3. **Ratio Prover/Verifier**:
   - Devrait rester stable (idéalement > 2.0)
   - Indique que la vérification reste rapide

4. **Taille du Circuit**:
   - Croissance linéaire = Normal
   - Croissance quadratique = Attention à la mémoire

## 🎯 Recommandations

### Pour la Recherche
- Inclure les 4 graphiques dans la publication
- Utiliser le tableau LaTeX
- Mentionner les R² dans le texte
- Discuter les implications de la complexité trouvée

### Pour l'Optimisation
- Si quadratique: Envisager des optimisations de circuit
- Si exponentielle: Refactoriser l'algorithme
- Comparer avec d'autres systèmes ZK (Groth16, PLONK, etc.)

## 📚 Références

- Google Benchmark: https://github.com/google/benchmark
- Scipy curve_fit: https://docs.scipy.org/doc/scipy/reference/generated/scipy.optimize.curve_fit.html
- Coefficient de détermination: https://en.wikipedia.org/wiki/Coefficient_of_determination
