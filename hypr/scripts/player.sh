#!/bin/bash

# This script gets the current song information using playerctl
# and formats it for display in Hyprlock.

# Define glyphs for playing and paused states
PLAYING_GLYPH="▶ " # Example playing glyph (you can change this)
PAUSED_GLYPH="⏸ "  # Example paused glyph (you can change this)

# Get metadata (artist and title) from the currently active player.
# Use --format to specify the output format.
# The 'status' subcommand is checked first to see if a player is running and playing.
# If playerctl fails (no player, or no metadata), it will exit with a non-zero status.
# The '2>/dev/null' redirects error output to /dev/null to keep the output clean.
PLAYER_STATUS=$(playerctl status 2>/dev/null)

if [ "$PLAYER_STATUS" == "Playing" ]; then
    # Player is running and playing, try to get metadata
    METADATA=$(playerctl metadata --format '{{ artist }} - {{ title }}' 2>/dev/null)

    if [ -n "$METADATA" ]; then
        # Metadata was successfully retrieved and is not empty
        echo "${PLAYING_GLYPH} ${METADATA}" | cut -c1-25
    else
        # Player is running but no metadata available (e.g., player open but no song)
        echo "${PLAYING_GLYPH} No track info. Idk what's playing"
    fi
elif [ "$PLAYER_STATUS" == "Paused" ]; then
    # Player is running but paused, try to get metadata
    METADATA=$(playerctl metadata --format '{{ artist }} - {{ title }}' 2>/dev/null)

    if [ -n "$METADATA" ]; then
        # Metadata was successfully retrieved and is not empty
        echo "${PAUSED_GLYPH} ${METADATA}" | cut -c1-25
    else
        # Player is running but no metadata available (e.g., player open but no song)
        echo "${PAUSED_GLYPH} No track info. Idk what's playing"
    fi
else
    # No player is running or recognized by playerctl
    echo "Kinda silent. No music"
fi
