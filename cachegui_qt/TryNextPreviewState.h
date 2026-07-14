#pragma once

class TryNextPreviewState
{
public:
    void Start(int firstProxyRow);
    void Stop();

    bool IsActive() const;
    bool IsExhausted(int proxyRowCount) const;

    int TakeNextProxyRow();
    int Attempts() const;

private:
    static constexpr int MaximumAttemptsPerClick = 200;

    bool active_ = false;
    int nextProxyRow_ = 0;
    int attempts_ = 0;
};
