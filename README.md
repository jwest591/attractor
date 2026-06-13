# Attractor 

This repository contains [NLSpecs](#terminology) to build your own version of Attractor to create your own software factory.

Although bringing your own agentic loop and unified LLM SDK is not required to build your own Attractor, we highly recommend controlling the stack so you have a strong foundation.

## Specs

- [Attractor Specification](./attractor-spec.md)
- [Coding Agent Loop Specification](./coding-agent-loop-spec.md)
- [Unified LLM Client Specification](./unified-llm-spec.md)

## Examples

The `examples/` directory contains annotated `.dot` files that demonstrate the
DSL and pipeline features:

| File | What it shows |
|------|---------------|
| `feature-pipeline.dot` | Full showcase — graph attributes, subgraph scoped defaults, all core handler types (codergen, tool, conditional, wait.human), goal gates, retries, fidelity modes, model stylesheet, accelerator-key human gates |

See [BUILD.md](./BUILD.md#run-examples) for instructions on running examples
with the CLI.

## Building Attractor

Supply the following prompt to a modern coding agent (Claude Code, Codex, OpenCode, Amp, Cursor, etc):

```
codeagent> Implement Attractor as described by https://github.com/strongdm/attractor
```

## Terminology

- **NLSpec** (Natural Language Spec): a human-readable spec intended to be  directly usable by coding agents to implement/validate behavior.
