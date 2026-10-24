#pragma once
// Input Buffer online counter: your Cloudflare Worker URL (see server/README.md),
// no slash at the end. Empty = the online / offline count stays hidden.
// Kept in its own file so updates to main.cpp never overwrite it.
constexpr char const* ONLINE_URL = "https://inputbuffer-online.alquerez.workers.dev";
