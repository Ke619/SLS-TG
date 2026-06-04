#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QPixmap>
#include <QPainter>
#include <QTimer>
#include <QThread>
#include <QProcess>
#include <QStandardPaths>
#include <QDir>
#include <QDesktopServices>
#include <QUrl>
#include <QMouseEvent>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>

// Forward declarations
class MainWindow;

// Worker thread for ticket-grabber
class TicketWorker : public QThread {
    Q_OBJECT
public:
    QString cmd;
    void run() override {
        QProcess proc;
        proc.setWorkingDirectory(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation));
        proc.start("cmd.exe", {"/c", cmd});
        proc.waitForStarted();
        while (proc.waitForReadyRead(-1)) {
            QString line = proc.readAllStandardOutput();
            for (auto &l : line.split('\n')) {
                l = l.trimmed();
                if (!l.isEmpty()) emit lineReady(l);
            }
        }
        proc.waitForFinished(-1);
        emit finished(proc.exitCode());
    }
signals:
    void lineReady(QString line);
    void finished(int code);
};

class MainWindow : public QMainWindow {
    Q_OBJECT

    QString appDir;
    QLabel *logoLabel;
    QLabel *statusLabel;
    QLineEdit *usernameEdit;
    QLineEdit *passwordEdit;
    QLineEdit *appidEdit;
    QPushButton *generateBtn;

    QMediaPlayer *musicPlayer;
    QAudioOutput *musicOutput;
    QMediaPlayer *clickPlayer;
    QAudioOutput *clickOutput;
    QMediaPlayer *hoverPlayer;
    QAudioOutput *hoverOutput;
    QMediaPlayer *ticketPlayer;
    QAudioOutput *ticketOutput;
    QMediaPlayer *happyPlayer;
    QAudioOutput *happyOutput;
    QMediaPlayer *sadPlayer;
    QAudioOutput *sadOutput;

    QTimer *dotTimer;
    int dotCount = 0;
    bool errorSet = false;
    bool musicPlaying = false;
    bool holdingLogo = false;
    QTimer *holdTimer;

    QPixmap bgPixmap;
    QPixmap logoIdle, logoProcessing, logoSuccess, logoError;

    void loadMedia() {
        auto makePlayer = [&](QMediaPlayer *&p, QAudioOutput *&o, const QString &file) {
            p = new QMediaPlayer(this);
            o = new QAudioOutput(this);
            p->setAudioOutput(o);
            p->setSource(QUrl::fromLocalFile(appDir + "/" + file));
        };
        makePlayer(musicPlayer, musicOutput, "BGM.wav");
        makePlayer(clickPlayer, clickOutput, "Click.mp3");
        makePlayer(hoverPlayer, hoverOutput, "Hover.mp3");
        makePlayer(ticketPlayer, ticketOutput, "ticket.mp3");
        makePlayer(happyPlayer, happyOutput, "Happy.mp3");
        makePlayer(sadPlayer, sadOutput, "Sad.mp3");

        connect(musicPlayer, &QMediaPlayer::mediaStatusChanged, [this](QMediaPlayer::MediaStatus s) {
            if (s == QMediaPlayer::EndOfMedia) { musicPlayer->setPosition(0); musicPlayer->play(); }
        });
        connect(sadPlayer, &QMediaPlayer::mediaStatusChanged, [this](QMediaPlayer::MediaStatus s) {
            if (s == QMediaPlayer::EndOfMedia) generateBtn->setEnabled(true);
        });
    }

    void playSfx(QMediaPlayer *p) {
        if (!p) return;
        p->setPosition(0);
        p->play();
    }

    void setLogo(const QPixmap &px) {
        if (!px.isNull())
            logoLabel->setPixmap(px.scaled(400, 400, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    void setStatus(const QString &text, const QString &color, bool isError = false) {
        statusLabel->setText(text);
        statusLabel->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 13px;").arg(color));
        if (isError) {
            errorSet = true;
            generateBtn->setEnabled(false);
            setLogo(logoError);
            playSfx(sadPlayer);
        }
    }

    void startDotAnimation(const QString &prefix) {
        dotCount = 0;
        dotTimer->disconnect();
        connect(dotTimer, &QTimer::timeout, [this, prefix]() {
            dotCount = (dotCount % 3) + 1;
            statusLabel->setText(prefix + QString(".").repeated(dotCount));
            statusLabel->setStyleSheet("color: #e6cc00; font-weight: bold; font-size: 13px;");
        });
        dotTimer->start(500);
    }

    void stopDotAnimation() { dotTimer->stop(); }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        int b = 4;
        p.fillRect(rect(), Qt::black);
        if (!bgPixmap.isNull()) {
            QRect inner(b, b, width() - b*2, height() - b*2 - 45);
            p.drawPixmap(inner, bgPixmap.scaled(inner.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        }
    }

    void mousePressEvent(QMouseEvent *e) override {
        if (e->button() == Qt::LeftButton) windowHandle()->startSystemMove();
    }

public:
    MainWindow(const QString &dir) : appDir(dir) {
        setWindowTitle("SLS Ticket Grabber");
        setFixedSize(450, 700);
        setWindowFlags(Qt::FramelessWindowHint);
        setWindowIcon(QIcon(appDir + "/Icon.png"));

        bgPixmap = QPixmap(appDir + "/Bg.png");
        logoIdle = QPixmap(appDir + "/L0.png");
        logoProcessing = QPixmap(appDir + "/L1.png");
        logoError = QPixmap(appDir + "/L2.png");
        logoSuccess = QPixmap(appDir + "/L3.png");

        dotTimer = new QTimer(this);
        holdTimer = new QTimer(this);
        holdTimer->setSingleShot(true);
        connect(holdTimer, &QTimer::timeout, [this]() {
            if (musicPlaying) { musicPlayer->stop(); musicPlaying = false; }
            else { musicPlayer->play(); musicPlaying = true; }
        });

        loadMedia();
        setupUI();
        setAttribute(Qt::WA_TranslucentBackground);
    }

    void setupUI() {
        QWidget *central = new QWidget(this);
        setCentralWidget(central);
        central->setStyleSheet("background: transparent;");

        QVBoxLayout *vbox = new QVBoxLayout(central);
        vbox->setContentsMargins(20, 10, 20, 50);
        vbox->setSpacing(8);

        // Logo
        logoLabel = new QLabel();
        logoLabel->setAlignment(Qt::AlignCenter);
        logoLabel->setStyleSheet("background: transparent;");
        setLogo(logoIdle);
        logoLabel->installEventFilter(this);
        vbox->addWidget(logoLabel);

        // Status
        statusLabel = new QLabel("");
        statusLabel->setAlignment(Qt::AlignCenter);
        statusLabel->setStyleSheet("color: #e6cc00; font-weight: bold; font-size: 13px; background: transparent;");
        vbox->addWidget(statusLabel);

        // Fields
        QString entryStyle = "background: rgba(255,255,255,0.7); color: #000000; border: 3px solid #000000; padding: 6px; font-size: 13px;";
        usernameEdit = new QLineEdit(); usernameEdit->setPlaceholderText("Steam username"); usernameEdit->setStyleSheet(entryStyle);
        passwordEdit = new QLineEdit(); passwordEdit->setPlaceholderText("Steam password"); passwordEdit->setEchoMode(QLineEdit::Password); passwordEdit->setStyleSheet(entryStyle);
        appidEdit = new QLineEdit(); appidEdit->setPlaceholderText("App ID"); appidEdit->setStyleSheet(entryStyle);
        vbox->addWidget(usernameEdit);
        vbox->addWidget(passwordEdit);
        vbox->addWidget(appidEdit);

        // Generate button
        generateBtn = new QPushButton("GENERATE");
        generateBtn->setFixedHeight(44);
        generateBtn->setStyleSheet(
            "QPushButton { background: rgba(0,0,0,0.4); color: #ffffff; border: 2px solid #ffffff; border-radius: 22px; font-size: 15px; font-weight: bold; letter-spacing: 3px; }"
            "QPushButton:hover { background: rgba(255,255,255,0.2); }"
            "QPushButton:disabled { background: rgba(0,0,0,0.2); color: #888; border-color: #888; }"
        );
        connect(generateBtn, &QPushButton::clicked, this, &MainWindow::onGenerate);
        connect(generateBtn, &QPushButton::clicked, [this]() { playSfx(clickPlayer); });
        vbox->addWidget(generateBtn, 0, Qt::AlignCenter);
        generateBtn->setFixedWidth(220);

        vbox->addStretch();

        // Bottom row
        QHBoxLayout *bottom = new QHBoxLayout();
        bottom->setContentsMargins(0, 0, 0, 0);

        // Info button
        QPushButton *infoBtn = new QPushButton("i");
        infoBtn->setFixedSize(22, 22);
        infoBtn->setStyleSheet(
            "QPushButton { background: #5dade2; color: #ffffff; border: 2px solid #5dade2; border-radius: 11px; font-size: 11px; font-weight: bold; }"
            "QPushButton:hover { background: #85c1e9; }"
        );
        connect(infoBtn, &QPushButton::clicked, []() { QDesktopServices::openUrl(QUrl("https://github.com/Ke619/SLS-TG")); });
        connect(infoBtn, &QPushButton::clicked, [this]() { playSfx(clickPlayer); });

        // Version label
        QLabel *versionLabel = new QLabel("Build: 2026.06.04.rv1.1");
        versionLabel->setAlignment(Qt::AlignCenter);
        versionLabel->setStyleSheet("color: #ffffff; font-size: 9px; background: transparent;");

        // Close button
        QPushButton *closeBtn = new QPushButton("✕");
        closeBtn->setFixedSize(22, 22);
        closeBtn->setStyleSheet(
            "QPushButton { background: #cc2200; color: #ffffff; border: 2px solid #cc2200; border-radius: 11px; font-size: 11px; font-weight: bold; }"
            "QPushButton:hover { background: #ff3300; }"
        );
        connect(closeBtn, &QPushButton::clicked, this, &QMainWindow::close);
        connect(closeBtn, &QPushButton::clicked, [this]() { playSfx(clickPlayer); });

        bottom->addWidget(infoBtn);
        bottom->addStretch();
        bottom->addWidget(versionLabel);
        bottom->addStretch();
        bottom->addWidget(closeBtn);

        vbox->addLayout(bottom);
    }

    bool eventFilter(QObject *obj, QEvent *e) override {
        if (obj == logoLabel) {
            if (e->type() == QEvent::MouseButtonPress) {
                holdTimer->start(3000);
            } else if (e->type() == QEvent::MouseButtonRelease) {
                holdTimer->stop();
            }
        }
        return QMainWindow::eventFilter(obj, e);
    }

public slots:
    void onGenerate() {
        QString user = usernameEdit->text().trimmed();
        QString pass = passwordEdit->text();
        QString appid = appidEdit->text().trimmed();

        if (user.isEmpty() || pass.isEmpty() || appid.isEmpty()) {
            setStatus("REQUIRED DETAILS MISSING", "#ff3300");
            return;
        }

        errorSet = false;
        generateBtn->setEnabled(false);
        setLogo(logoProcessing);
        startDotAnimation("CONNECTING");

        QString binPath = appDir + "/ticket-grabber.exe";
        QString cmd = QString("\"%1\" \"%2\" \"%3\" \"%4\"").arg(binPath, user, pass, appid);

        TicketWorker *worker = new TicketWorker();
        worker->cmd = cmd;

        connect(worker, &TicketWorker::lineReady, this, [this](QString line) {
            if (line.contains("Connected to Steam")) {
                stopDotAnimation();
                setStatus("AWAITING STEAM GUARD AUTHENTICATION", "#e6cc00");
            } else if (line.contains("Logged in as")) {
                startDotAnimation("GENERATING YOUR TICKET");
                setLogo(logoProcessing);
            } else if (line.contains("Account Info received")) {
                startDotAnimation("GENERATING YOUR TICKET");
            } else if (line.contains("Saved")) {
                stopDotAnimation();
                setStatus("TICKET GENERATED!", "#228822");
                setLogo(logoSuccess);
                playSfx(ticketPlayer);
                playSfx(happyPlayer);
                generateBtn->setEnabled(true);
            } else if (line.contains("is not a number")) {
                stopDotAnimation(); setStatus("INVALID APP ID", "#ff3300", true);
            } else if (line.contains("Failed to get Handlers")) {
                stopDotAnimation(); setStatus("INTERNAL ERROR", "#ff3300", true);
            } else if (line.contains("Failed GetAppOwnershipTicket")) {
                stopDotAnimation(); setStatus("OWNERSHIP VERIFICATION FAILED", "#ff3300", true);
            } else if (line.contains("Failed RequestEncryptedAppTicket")) {
                stopDotAnimation(); setStatus("TICKET ENCRYPTION FAILED", "#ff3300", true);
            } else if (line.contains("Failed to receive both tickets")) {
                stopDotAnimation(); setStatus("STEAM CONNECTION ERROR", "#ff3300", true);
            } else if (line.contains("Disconnected from Steam")) {
                stopDotAnimation(); setStatus("DISCONNECTED FROM STEAM", "#ff3300", true);
            }
        });

        connect(worker, &TicketWorker::finished, this, [this](int code) {
            stopDotAnimation();
            if (code != 0 && !errorSet) {
                setStatus("CRITICAL ERROR", "#ff3300", true);
            }
            generateBtn->setEnabled(true);
        });

        connect(worker, &TicketWorker::finished, worker, &QObject::deleteLater);
        worker->start();
    }
};

#include "main.moc"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QString appDir = QCoreApplication::applicationDirPath();
    MainWindow w(appDir);
    w.show();
    return app.exec();
}
