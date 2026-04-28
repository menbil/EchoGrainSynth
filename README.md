# EchoGrainFX

## Présentation
EchoGrainFX est un plugin audio développé avec JUCE. Ce dépôt contient tout le nécessaire pour ouvrir, compiler et utiliser le projet.

## Prérequis

- **JUCE** (inclus dans le dossier JuceLibraryCode)
- **Projucer** (éditeur de projet JUCE)
- Un IDE compatible (Visual Studio, Xcode, etc.)

## Installation & Compilation

1. **Cloner le dépôt**
   ```sh
   git clone https://github.com/ton-utilisateur/EchoGrainFX.git
   cd EchoGrainFX
   ```

2. **Ouvrir le projet avec Projucer**
   - Lance Projucer.
   - Ouvre le fichier `EchoGrainFX.jucer`.
   - Vérifie les chemins et les modules si besoin.

3. **Exporter vers ton IDE**
   - Dans Projucer, choisis l’exportateur (Visual Studio, Xcode, etc.).
   - Clique sur “Save Project and Open in IDE”.

4. **Compiler**
   - Compile le projet dans ton IDE.
   - Le plugin sera généré dans le dossier Builds/ correspondant à ton IDE.

## Utilisation

- Charge le plugin dans ton DAW compatible (VST, AU, etc.).
- Profite des fonctionnalités d’EchoGrainFX !

## Remarques

- Si tu modifies le .jucer, pense à régénérer les fichiers de projet via Projucer.
- Le dossier JuceLibraryCode est inclus, donc aucune dépendance externe n’est requise.
