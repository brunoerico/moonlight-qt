#!/usr/bin/env python3
"""Confere se as fontes embutidas têm todos os caracteres que o HUD e o menu da LanHouse desenham.

O SDL_ttf não tem fallback de fonte: um caractere ausente vira um retângulo. Foi assim que o "∞" (plano
ilimitado) apareceu como uma barra quebrada quando a Inter foi embutida só com o subset 'latin' (2026-09-26).
Rode depois de trocar qualquer fonte: python3 fonts/check-glyphs.py
"""
import sys
from fontTools.ttLib import TTFont

USADOS = "0123456789 .,:x%·—∞↑↓ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyzáàâãéêíóôõúçÁÉÍÓÚÇ"
ok = True
for nome in ("Inter-SemiBold.ttf", "Inter-Bold.ttf"):
    cmap = TTFont(f"{__file__.rsplit('/', 1)[0]}/{nome}").getBestCmap()
    faltam = sorted({c for c in USADOS if ord(c) not in cmap})
    print(nome, "OK" if not faltam else f"FALTAM: {faltam}")
    ok = ok and not faltam
sys.exit(0 if ok else 1)
