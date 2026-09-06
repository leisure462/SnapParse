#include "SensitiveDetector.h"

bool SensitiveDetector::isSensitive(const QString& text) {
    QString trimmed = text.trimmed();
    if (trimmed.length() < 12) return false;

    // Comprehensive Industry-Grade API Keys & Secrets Regex Matrix
    static const QList<QRegularExpression> patterns = {
        // 1. AI & LLM Model Keys (OpenAI, Claude, Gemini, DeepSeek, Kimi, Baidu, GLM, Groq, HF, etc.)
        QRegularExpression(R"(sk-[a-zA-Z0-9_\-\.]{20,})"),                        // OpenAI / DeepSeek / Moonshot / GLM
        QRegularExpression(R"(AIzaSy[a-zA-Z0-9_\-]{33})"),                        // Google Gemini / API Key
        QRegularExpression(R"(gsk_[a-zA-Z0-9]{48})"),                             // Groq API Key
        QRegularExpression(R"(hf_[a-zA-Z0-9]{34})"),                              // HuggingFace Token
        QRegularExpression(R"(r8_[a-zA-Z0-9]{32,})"),                             // Replicate API Token

        // 2. Cloud Providers (AWS, AliCloud, Tencent, Azure, GCP)
        QRegularExpression(R"(AKIA[0-9A-Z]{16})"),                                // AWS Access Key ID
        QRegularExpression(R"(LTAI[0-9a-zA-Z]{16,24})"),                          // AliCloud AccessKey
        QRegularExpression(R"(AKID[0-9a-zA-Z]{16,32})"),                          // Tencent Cloud SecretId

        // 3. Git & DevOps (GitHub, GitLab, Slack, NPM, PyPI)
        QRegularExpression(R"(gh[pousr]_[a-zA-Z0-9]{36,255})"),                   // GitHub Token (PAT, OAuth, etc.)
        QRegularExpression(R"(github_pat_[a-zA-Z0-9]{22}_[a-zA-Z0-9]{59})"),      // GitHub Fine-grained PAT
        QRegularExpression(R"(glpat-[a-zA-Z0-9\-_]{20,})"),                       // GitLab PAT
        QRegularExpression(R"(npm_[a-zA-Z0-9]{36})"),                             // NPM Access Token
        QRegularExpression(R"(pypi-AgEIcHlwaS5vcmc[A-Za-z0-9\-_]{50,})"),         // PyPI Token
        QRegularExpression(R"(xox[baprs]-[0-9a-zA-Z_\-]{10,})"),                  // Slack Token

        // 4. Web, Auth & Payment (Stripe, JWT, Bearer)
        QRegularExpression(R"([sr]k_(?:live|test)_[0-9a-zA-Z]{24,99})"),          // Stripe API Key
        QRegularExpression(R"(ey[A-Za-z0-9-_=]+\.[A-Za-z0-9-_=]+\.?[A-Za-z0-9-_.+/=]*)"), // JWT Token
        QRegularExpression(R"((?i)bearer\s+[a-zA-Z0-9_\-\.]{20,})"),              // Bearer Token

        // 5. Cryptographic Keys & Certificates
        QRegularExpression(R"(-----BEGIN [A-Z0-9 -]+(?:PRIVATE KEY|CERTIFICATE)-----)"), // PEM Private Key
        QRegularExpression(R"(ssh-rsa\s+AAAA[0-9A-Za-z+/]+[=]{0,3})"),           // SSH Key

        // 6. Config / Key-Value Secret Pairs (JSON, YAML, .env, URL query)
        QRegularExpression(R"((?i)["']?(?:password|passwd|pwd|secret|token|api[_-]?key|apikey|app[_-]?secret|access[_-]?token|refresh[_-]?token|private[_-]?key)["']?\s*[:=]\s*["']?([^"',\s\r\n]{6,})["']?)"),

        // 7. Database URI with Password
        QRegularExpression(R"((?:postgres|mysql|mongodb|redis|amqp|mssql)://[^\s:]+:([^\s@]+)@)"),

        // 8. High-Entropy Standalone API / Secret Hex & Base64 Hashes (32 to 64 chars)
        QRegularExpression(R"(^(?:[0-9a-fA-F]{32}|[0-9a-fA-F]{40}|[0-9a-fA-F]{64})$)"),
        QRegularExpression(R"((?i)^(?:key|sec|token)[-_][a-zA-Z0-9_\-]{16,}$)"),
        QRegularExpression(R"(^[a-zA-Z0-9_\-]{32,80}$)")
    };

    for (const auto& pattern : patterns) {
        if (pattern.match(trimmed).hasMatch()) {
            return true;
        }
    }

    return false;
}

QString SensitiveDetector::redact(const QString& text) {
    QString trimmed = text.trimmed();
    int len = trimmed.length();
    if (len <= 8) {
        return "••••••••";
    }
    if (len <= 14) {
        return trimmed.left(2) + " •••••••• " + trimmed.right(2);
    }
    return trimmed.left(4) + " •••••••••••••••• " + trimmed.right(4);
}
