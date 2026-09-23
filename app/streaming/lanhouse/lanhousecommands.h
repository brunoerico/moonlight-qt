#pragma once

// LanHouse - canal de comandos do app wrapper (Windows/Mac) pro Moonlight.
//
// O wrapper roda logado no site e o Moonlight só tem a chave anônima + o id da
// sessão (ver cli/lanhouseconnect.cpp), então quem sabe quantos minutos ainda
// restam é o wrapper. Ele manda pelo stdin do Moonlight, uma linha por vez:
//
//   minutes 87.9        -> restam 87,9 minutos (o HUD mostra 87min)
//   minutes unlimited   -> plano ilimitado (o HUD mostra ∞)
//   minutes unknown     -> não sabe (o HUD mostra —)
//
// Linha desconhecida é ignorada. Sem wrapper (stdin fechado/terminal), nada
// acontece e o HUD mostra "—" nos minutos.
namespace LanhouseCommands {

// Idempotente. Lê o stdin numa thread própria e nunca bloqueia o streaming.
void startStdinReader();

}
