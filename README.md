# RecallAID - Unreal Engine Plugin

**RecallAID** (Recall AI Director) is an AI Director system built on top of Unreal Engine's Mass ECS framework, providing intelligent enemy spawning, dynamic difficulty adjustment, and adaptive gameplay management for deterministic multiplayer games.

> **Note**: This plugin was formerly known as **MassStepAID**. It has been ported from **MassExtended** (a custom, forked version of Mass) to Unreal Engine's stock **Mass** framework directly, and renamed to align with the Recall framework naming convention.

## About RecallAID

RecallAID is an AI management system that extends Unreal's Mass ECS framework with intelligent game direction capabilities. It offers:

- **Intelligent Enemy Spawning**: Dynamic enemy spawn management based on player behavior and game state
- **Adaptive Difficulty**: Real-time difficulty adjustment to maintain optimal player engagement
- **Spawn Point Management**: Advanced spawn point selection and optimization systems
- **State Management**: Comprehensive AI Director state tracking and decision making
- **Performance Optimization**: Efficient spawn management for large-scale encounters
- **Deterministic Behavior**: Fully deterministic AI Director decisions for multiplayer consistency

## Plugin Description

*An AI Director that manages enemy spawn and more*

## Origin and Development

- **Formerly**: MassStepAID (built on MassExtended, a custom fork of Mass)
- **Now**: RecallAID (built on Mass, Unreal Engine's stock ECS framework)
- **Author**: Bastien Van de Walle

## Core Modules

### RecallAIDCore (Runtime)
Core AI Director framework and data management: AI Director assets, spawn point settings, spawn types, and configuration framework.

### RecallAID (Runtime)
AI Director execution and processing systems: spawn management, spawn point processing, entity management, state processing, and StateTree integration.

## Module Dependencies

### Required Plugins
- **Recall**: Core ECS framework with rollback support (Required)
- **RecallGameplay**: Gameplay systems integration for spawn coordination (Required)
- **VariableCollection**: Dynamic configuration management (Required)
- **StateTree**: AI and behavior systems (Required)

## Configuration

### Basic Setup
1. **Enable Plugin**: Enable RecallAID in project plugins (disabled by default)
2. **Configure Dependencies**: Ensure Recall, RecallGameplay, and other dependencies are enabled
3. **Setup AI Director Assets**: Configure AI Director behavior and spawn settings
4. **Initialize Spawn Points**: Set up spawn point locations and configurations

## Developer Notes

- **Author**: Bastien Van de Walle
- **Category**: Gameplay
- **Version**: 1.0
- **Default State**: Disabled by default (opt-in for projects requiring AI Director)
- **Content Support**: Enabled for AI Director assets and configurations
- **Framework**: Built on Mass ECS with intelligent spawn management

For historical context on the original design (under MassStepAID/MassExtended), see [README_old.md](README_old.md).