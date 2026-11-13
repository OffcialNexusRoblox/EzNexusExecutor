#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <QGroupBox>
#include <QProgressBar>
#include <QTimer>
#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <iostream>
#include <vector>

// --- Advanced Injector Class ---
class ExecutorCore {
public:
    DWORD robloxPID = 0;
    bool hyperionActive = false;
    bool autoInject = false;
    
    // Callback for logging to UI
    void (*logCallback)(const std::string& msg) = nullptr;

    void log(const std::string& msg) {
        if (logCallback) logCallback(msg);
    }

    DWORD findRoblox() {
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot == INVALID_HANDLE_VALUE) return 0;

        PROCESSENTRY32 pe;
        pe.dwSize = sizeof(pe);

        if (ProcessFirst(hSnapshot, &pe)) {
            do {
                std::wstring wName(pe.szExeFile);
                if (wName.find(L"RobloxPlayerBeta") != std::wstring::npos || 
                    wName.find(L"Roblox") != std::wstring::npos) {
                    CloseHandle(hSnapshot);
                    return pe.th32ProcessID;
                }
            } while (ProcessNext(hSnapshot, &pe));
        }
        CloseHandle(hSnapshot);
        return 0;
    }

    bool checkHyperion() {
        // Check for common Hyperion DLLs or signatures
        // This is a simplified check; real executors scan memory patterns
        HMODULE hMod = GetModuleHandle(L"RobloxPlayerBeta.exe");
        if (hMod) {
            // If base address is in high memory space, likely Hyperion
            if ((uintptr_t)hMod > 0x7FFF00000000ULL) {
                hyperionActive = true;
                log("[+] Hyperion Detected!");
                return true;
            }
        }
        hyperionActive = false;
        return false;
    }

    bool injectLua(const std::string& luaCode) {
        if (robloxPID == 0) {
            robloxPID = findRoblox();
            if (robloxPID == 0) {
                log("[-] Roblox not found.");
                return false;
            }
        }

        log("[*] Opening process...");
        HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, robloxPID);
        if (!hProcess) {
            log("[-] Failed to open process.");
            return false;
        }

        // Allocate memory
        LPVOID addr = VirtualAllocEx(hProcess, NULL, luaCode.size(), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (!addr) {
            log("[-] Memory allocation failed.");
            CloseHandle(hProcess);
            return false;
        }

        // Write code
        SIZE_T bytesWritten;
        if (!WriteProcessMemory(hProcess, addr, luaCode.c_str(), luaCode.size(), &bytesWritten)) {
            log("[-] Write failed.");
            CloseHandle(hProcess);
            return false;
        }

        // Execute
        HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)addr, NULL, 0, NULL);
        if (hThread) {
            WaitForSingleObject(hThread, 3000);
            CloseHandle(hThread);
            log("[+] Injection Successful!");
        } else {
            log("[-] Thread creation failed.");
        }

        CloseHandle(hProcess);
        return true;
    }

    void monitorLoop() {
        while (true) {
            DWORD currentPID = findRoblox();
            if (currentPID != robloxPID && robloxPID != 0) {
                log("[-] Roblox crashed or updated. Resetting...");
                robloxPID = 0;
            }
            if (currentPID != 0 && robloxPID == 0) {
                robloxPID = currentPID;
                log("[+] Roblox detected! PID: " + std::to_string(robloxPID));
                if (autoInject) {
                    // Auto-inject default script if available
                    injectLua("print('Auto-Injected by Nova')");
                }
            }
            checkHyperion();
            Sleep(1000); // Check every second
        }
    }
};

// Global instance for callbacks
ExecutorCore core;

// --- UI Classes ---
class MainWindow : public QWidget {
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr) : QWidget(parent) {
        setWindowTitle("Nova Ultimate Executor");
        resize(900, 700);

        // Layout
        mainLayout = new QVBoxLayout(this);

        // Top Bar
        topBar = new QHBoxLayout();
        statusLabel = new QLabel("Status: Offline");
        statusLabel->setStyleSheet("color: #ffcc00; font-weight: bold;");
        
        autoInjectCheck = new QCheckBox("Auto-Inject");
        autoInjectCheck->setChecked(true);
        
        topBar->addWidget(statusLabel);
        topBar->addStretch();
        topBar->addWidget(autoInjectCheck);
        mainLayout->addLayout(topBar);

        // Script Editor
        editorGroup = new QGroupBox("Script Editor");
        editorLayout = new QVBoxLayout();
        scriptEditor = new QTextEdit();
        scriptEditor->setPlaceholderText("-- Enter your Lua code here...\nprint('Hello World')");
        scriptEditor->setStyleSheet("background-color: #1e1e1e; color: #d4d4d4; border: 1px solid #333; border-radius: 5px; padding: 10px;");
        editorLayout->addWidget(scriptEditor);
        editorGroup->setLayout(editorLayout);
        mainLayout->addWidget(editorGroup);

        // Buttons
        btnLayout = new QHBoxLayout();
        injectButton = new QPushButton("INJECT");
        injectButton->setStyleSheet("background-color: #007acc; color: white; font-weight: bold; padding: 10px; border-radius: 5px;");
        
        clearButton = new QPushButton("Clear Console");
        clearButton->setStyleSheet("background-color: #333; color: white; padding: 10px; border-radius: 5px;");
        
        btnLayout->addWidget(injectButton);
        btnLayout->addWidget(clearButton);
        mainLayout->addLayout(btnLayout);

        // Console Output
        consoleGroup = new QGroupBox("Console Log");
        consoleLayout = new QVBoxLayout();
        consoleOutput = new QTextEdit();
        consoleOutput->setReadOnly(true);
        consoleOutput->setStyleSheet("background-color: #0a0a0a; color: #00ff00; font-family: Consolas; border: 1px solid #333;");
        consoleLayout->addWidget(consoleOutput);
        consoleGroup->setLayout(consoleLayout);
        mainLayout->addWidget(consoleGroup);

        // Set Callbacks
        core.logCallback = [this](const std::string& msg) {
            QString qMsg = QString::fromStdString(msg);
            this->consoleOutput->append(qMsg);
            this->consoleOutput->verticalScrollBar()->setValue(this->consoleOutput->verticalScrollBar()->maximum());
        };

        // Connect Signals
        connect(injectButton, &QPushButton::clicked, this, &MainWindow::inject);
        connect(clearButton, &QPushButton::clicked, this, &MainWindow::clearConsole);
        connect(autoInjectCheck, &QCheckBox::toggled, this, [this](bool checked) {
            core.autoInject = checked;
        });

        // Start Monitor Thread
        QTimer *timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, [this]() {
            // Simple loop to update status
            DWORD pid = core.findRoblox();
            if (pid != 0) {
                statusLabel->setText("Status: Online (PID: " + std::to_string(pid) + ")");
                statusLabel->setStyleSheet("color: #4caf50; font-weight: bold;");
            } else {
                statusLabel->setText("Status: Offline");
                statusLabel->setStyleSheet("color: #ffcc00; font-weight: bold;");
            }
        });
        timer->start(1000);
    }

private slots:
    void inject() {
        QString script = scriptEditor->toPlainText();
        std::string luaStr = script.toStdString();
        core.injectLua(luaStr);
    }

    void clearConsole() {
        consoleOutput->clear();
    }

private:
    QVBoxLayout *mainLayout;
    QHBoxLayout *topBar;
    QLabel *statusLabel;
    QCheckBox *autoInjectCheck;
    
    QGroupBox *editorGroup;
    QVBoxLayout *editorLayout;
    QTextEdit *scriptEditor;
    
    QHBoxLayout *btnLayout;
    QPushButton *injectButton;
    QPushButton *clearButton;
    
    QGroupBox *consoleGroup;
    QVBoxLayout *consoleLayout;
    QTextEdit *consoleOutput;
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setStyleSheet("QWidget { background-color: #121212; }");

    MainWindow window;
    window.show();
    return app.exec();
}

#include "main.moc"