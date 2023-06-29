#pragma once
#include <optional>

class SyncState {
public:
    void set_block_download(bool val)
    {
        blockDownloadActive = val;
    }

    void set_header_download(bool val)
    {
        headerDownloadActive = val;
    }

    void set_has_connections(bool hasConnections)
    {
        hasInitializedConnections = hasConnections;
    }

    [[nodiscard]] std::optional<bool> detect_change()
    {
        bool newState;
        if (oldState == true) {
            newState = hasInitializedConnections;
        } else {
            newState = hasInitializedConnections && !blockDownloadActive && !headerDownloadActive;
        }
        bool changed = (newState != oldState);
        oldState = newState;
        if (changed) {
            return newState;
        }
        return {};
    }

private:
    bool hasInitializedConnections { false };
    bool blockDownloadActive { false };
    bool headerDownloadActive { false };
    bool oldState { false };
};
