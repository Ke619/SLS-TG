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
#include <QWindow>
#include <QFile>

class TicketWorker : public QThread {
    Q_OBJECT
public:
    QString binPath;
    QString username;
    QString password;
    QString appid;
    QString workDir;

    void run() override {
        QProcess proc;
        proc.setWorkingDirectory(workDir);
        proc.setProcessChannelMode(QProcess::MergedChannels);
        // Pass args directly as list - shell never sees them, & in password is safe
        proc.start(binPath, {username, password, appid});
        if (!proc.waitForStarted(5000)) {
            emit lineReady("[ ERROR: Failed to start ticket-grabber ]");
            emit done(1);
            return;
        }
        while (proc.state() != QProcess::NotRunning) {
            proc.waitForReadyRead(200);
            QString out = proc.readAll();
            for (auto l : out.split('\n')) {
                l = l.trimmed().remove('\r');
                if (!l.isEmpty()) emit lineReady(l);
            }
        }
        QString remaining = proc.readAll();
        for (auto l : remaining.split('\n')) {
            l = l.trimmed().remove('\r');
            if (!l.isEmpty()) emit lineReady(l);
        }
        emit done(proc.exitCode());
    }
signals:
    void lineReady(QString line);
    void done(int code);
};

class MainWindow : public QMainWindow {
    Q_OBJECT

    QString appDir;
    QLabel *logoLabel;
    QLabel *statusLabel;
    QLineEdit *usernameEdit, *passwordEdit, *appidEdit;
    QPushButton *generateBtn;

    QMediaPlayer *musicPlayer; QAudioOutput *musicOut;
    QMediaPlayer *clickPlayer; QAudioOutput *clickOut;
    QMediaPlayer *hoverPlayer; QAudioOutput *hoverOut;
    QMediaPlayer *ticketPlayer; QAudioOutput *ticketOut;
    QMediaPlayer *happyPlayer; QAudioOutput *happyOut;
    QMediaPlayer *sadPlayer; QAudioOutput *sadOut;

    QTimer *dotTimer;
    int dotCount = 0;
    bool errorSet = false;
    bool musicPlaying = false;
    QTimer *holdTimer;

    QPixmap bgPixmap;
    QPixmap logoIdle, logoProcessing, logoSuccess, logoError;

    QMediaPlayer* makePlayer(QAudioOutput *&out, const QString &file) {
        auto *p = new QMediaPlayer(this);
        out = new QAudioOutput(this);
        p->setAudioOutput(out);
        QString path = appDir + "/" + file;
        if (QFile::exists(path))
            p->setSource(QUrl::fromLocalFile(path));
        return p;
    }

    void playSfx(QMediaPlayer *p) {
        if (!p) return;
        p->setPosition(0);
        p->play();
    }

    void setLogo(const QPixmap &px) {
        if (!px.isNull())
            logoLabel->setPixmap(px.scaled(380, 380, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    void setStatus(const QString &text, const QString &color, bool isError = false) {
        dotTimer->stop();
        statusLabel->setText(text);
        statusLabel->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 13px; background: transparent;").arg(color));
        if (isError) {
            errorSet = true;
            setLogo(logoError);
            generateBtn->setEnabled(false);
            playSfx(sadPlayer);
            connect(sadPlayer, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus s) {
                if (s == QMediaPlayer::EndOfMedia) generateBtn->setEnabled(true);
            }, Qt::UniqueConnection);
        }
    }

    void startDots(const QString &prefix) {
        dotCount = 0;
        dotTimer->disconnect();
        connect(dotTimer, &QTimer::timeout, this, [this, prefix]() {
            dotCount = (dotCount % 3) + 1;
            statusLabel->setText(prefix + QString(".").repeated(dotCount));
            statusLabel->setStyleSheet("color: #e6cc00; font-weight: bold; font-size: 13px; background: transparent;");
        });
        dotTimer->start(500);
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        int b = 4, bot = 45;
        p.fillRect(rect(), Qt::black);
        if (!bgPixmap.isNull()) {
            QRect inner(b, b, width()-b*2, height()-b-bot);
            p.drawPixmap(inner, bgPixmap.scaled(inner.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        }
    }

    void mousePressEvent(QMouseEvent *e) override {
        if (e->button() == Qt::LeftButton && windowHandle())
            windowHandle()->startSystemMove();
    }

    bool eventFilter(QObject *obj, QEvent *e) override {
        if (obj == logoLabel) {
            if (e->type() == QEvent::MouseButtonPress) holdTimer->start(3000);
            else if (e->type() == QEvent::MouseButtonRelease) holdTimer->stop();
        }
        return QMainWindow::eventFilter(obj, e);
    }

public:
    MainWindow(const QString &dir) : appDir(dir) {
        setWindowTitle("SLS Ticket Grabber");
        setFixedSize(450, 700);
        setWindowFlags(Qt::FramelessWindowHint);
        setAttribute(Qt::WA_TranslucentBackground);
        setWindowIcon(QIcon(appDir + "/Icon.png"));

        bgPixmap      = QPixmap(appDir + "/Bg.png");
        logoIdle      = QPixmap(appDir + "/L0.png");
        logoProcessing= QPixmap(appDir + "/L1.png");
        logoError     = QPixmap(appDir + "/L2.png");
        logoSuccess   = QPixmap(appDir + "/L3.png");

        dotTimer  = new QTimer(this);
        holdTimer = new QTimer(this);
        holdTimer->setSingleShot(true);

        musicPlayer = makePlayer(musicOut,  "BGM.wav");
        clickPlayer = makePlayer(clickOut,  "Click.mp3");
        hoverPlayer = makePlayer(hoverOut,  "Hover.mp3");
        ticketPlayer= makePlayer(ticketOut, "ticket.mp3");
        happyPlayer = makePlayer(happyOut,  "Happy.mp3");
        sadPlayer   = makePlayer(sadOut,    "Sad.mp3");

        connect(musicPlayer, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus s) {
            if (s == QMediaPlayer::EndOfMedia) { musicPlayer->setPosition(0); musicPlayer->play(); }
        });
        connect(holdTimer, &QTimer::timeout, this, [this]() {
            if (musicPlaying) { musicPlayer->stop(); musicPlaying = false; }
            else { musicPlayer->play(); musicPlaying = true; }
        });

        setupUI();
    }

    void setupUI() {
        QWidget *central = new QWidget(this);
        setCentralWidget(central);
        central->setStyleSheet("background: transparent;");

        QVBoxLayout *vbox = new QVBoxLayout(central);
        vbox->setContentsMargins(20, 10, 20, 50);
        vbox->setSpacing(8);

        logoLabel = new QLabel();
        logoLabel->setAlignment(Qt::AlignCenter);
        logoLabel->setStyleSheet("background: transparent;");
        logoLabel->setFixedHeight(390);
        setLogo(logoIdle);
        logoLabel->installEventFilter(this);
        vbox->addWidget(logoLabel);

        statusLabel = new QLabel("");
        statusLabel->setAlignment(Qt::AlignCenter);
        statusLabel->setStyleSheet("color: #e6cc00; font-weight: bold; font-size: 13px; background: transparent;");
        vbox->addWidget(statusLabel);

        QString es = "background: rgba(255,255,255,180); color: #000000; border: 3px solid #000000; padding: 6px; font-size: 13px;";
        usernameEdit = new QLineEdit(); usernameEdit->setPlaceholderText("Steam username"); usernameEdit->setStyleSheet(es);
        passwordEdit = new QLineEdit(); passwordEdit->setPlaceholderText("Steam password"); passwordEdit->setEchoMode(QLineEdit::Password); passwordEdit->setStyleSheet(es);
        appidEdit    = new QLineEdit(); appidEdit->setPlaceholderText("App ID"); appidEdit->setStyleSheet(es);
        vbox->addWidget(usernameEdit);
        vbox->addWidget(passwordEdit);
        vbox->addWidget(appidEdit);

        generateBtn = new QPushButton("GENERATE");
        generateBtn->setFixedSize(220, 44);
        generateBtn->setStyleSheet(
            "QPushButton{background:rgba(0,0,0,100);color:#ffffff;border:2px solid #ffffff;border-radius:22px;font-size:15px;font-weight:bold;letter-spacing:3px;}"
            "QPushButton:hover{background:rgba(255,255,255,50);}"
            "QPushButton:disabled{background:rgba(0,0,0,50);color:#888;border-color:#888;}"
        );
        connect(generateBtn, &QPushButton::clicked, this, &MainWindow::onGenerate);
        connect(generateBtn, &QPushButton::clicked, this, [this](){ playSfx(clickPlayer); });
        vbox->addWidget(generateBtn, 0, Qt::AlignCenter);
        vbox->addStretch();

        QHBoxLayout *bot = new QHBoxLayout();
        QPushButton *infoBtn = new QPushButton("i");
        infoBtn->setFixedSize(22, 22);
        infoBtn->setStyleSheet("QPushButton{background:#5dade2;color:#fff;border:2px solid #5dade2;border-radius:11px;font-size:11px;font-weight:bold;}QPushButton:hover{background:#85c1e9;}");
        connect(infoBtn, &QPushButton::clicked, [](){ QDesktopServices::openUrl(QUrl("https://github.com/Ke619/SLS-TG")); });
        connect(infoBtn, &QPushButton::clicked, this, [this](){ playSfx(clickPlayer); });

        QLabel *verLabel = new QLabel("Build: 2026.06.04.rv1.1");
        verLabel->setAlignment(Qt::AlignCenter);
        verLabel->setStyleSheet("color:#ffffff;font-size:9px;background:transparent;");

        QPushButton *closeBtn = new QPushButton("✕");
        closeBtn->setFixedSize(22, 22);
        closeBtn->setStyleSheet("QPushButton{background:#cc2200;color:#fff;border:2px solid #cc2200;border-radius:11px;font-size:11px;font-weight:bold;}QPushButton:hover{background:#ff3300;}");
        connect(closeBtn, &QPushButton::clicked, this, &QMainWindow::close);
        connect(closeBtn, &QPushButton::clicked, this, [this](){ playSfx(clickPlayer); });

        bot->addWidget(infoBtn);
        bot->addStretch();
        bot->addWidget(verLabel);
        bot->addStretch();
        bot->addWidget(closeBtn);
        vbox->addLayout(bot);
    }

public slots:
    void onGenerate() {
        QString user  = usernameEdit->text().trimmed();
        QString pass  = passwordEdit->text();
        QString appid = appidEdit->text().trimmed();

        if (user.isEmpty() || pass.isEmpty() || appid.isEmpty()) {
            setStatus("REQUIRED DETAILS MISSING", "#ff3300");
            return;
        }

        errorSet = false;
        generateBtn->setEnabled(false);
        setLogo(logoProcessing);
        startDots("CONNECTING");

        QString downloads = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        QDir().mkpath(downloads);

        auto *worker = new TicketWorker();
        worker->binPath  = appDir + "/ticket-grabber.exe";
        worker->username = user;
        worker->password = pass;
        worker->appid    = appid;
        worker->workDir  = downloads;

        connect(worker, &TicketWorker::lineReady, this, [this](QString line) {
            if (line.contains("APPROVE STEAM GUARD"))
                { dotTimer->stop(); setStatus("AWAITING STEAM GUARD AUTHENTICATION", "#e6cc00"); }
            else if (line.contains("Connected to Steam"))
                { dotTimer->stop(); setStatus("AWAITING STEAM GUARD AUTHENTICATION", "#e6cc00"); }
            else if (line.contains("Logged in as"))
                { startDots("GENERATING YOUR TICKET"); setLogo(logoProcessing); }
            else if (line.contains("Saved"))
                { dotTimer->stop(); setStatus("TICKET GENERATED!", "#228822"); setLogo(logoSuccess); playSfx(ticketPlayer); playSfx(happyPlayer); generateBtn->setEnabled(true); }
            else if (line.contains("is not a number"))
                setStatus("INVALID APP ID", "#ff3300", true);
            else if (line.contains("Failed to get Handlers"))
                setStatus("INTERNAL ERROR", "#ff3300", true);
            else if (line.contains("Failed GetAppOwnershipTicket"))
                setStatus("OWNERSHIP VERIFICATION FAILED", "#ff3300", true);
            else if (line.contains("Failed RequestEncryptedAppTicket"))
                setStatus("TICKET ENCRYPTION FAILED", "#ff3300", true);
            else if (line.contains("Failed to receive both tickets"))
                setStatus("STEAM CONNECTION ERROR", "#ff3300", true);
            else if (line.contains("Disconnected from Steam"))
                setStatus("DISCONNECTED FROM STEAM", "#ff3300", true);
        });

        connect(worker, &TicketWorker::done, this, [this](int code) {
            dotTimer->stop();
            if (code != 0 && !errorSet)
                setStatus("CRITICAL ERROR", "#ff3300", true);
            if (code == 0 && !errorSet)
                generateBtn->setEnabled(true);
        });

        connect(worker, &TicketWorker::done, worker, &QObject::deleteLater);
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
