/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2021 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include <KIO/JobUiDelegateFactory>
#include <KTerminalLauncherJob>

#include <KService>

#include <QApplication>
#include <QDebug>
#include <QProcessEnvironment>
#include <QStandardPaths>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QString command;
    if (argc > 1) {
        command = QString::fromLocal8Bit(argv[1]);
    }

    auto *job = new KTerminalLauncherJob(command);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("MYVAR"), QStringLiteral("myvalue")); // for interactive testing that it was set
    job->setProcessEnvironment(env);
    job->setWorkingDirectory(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)); // for testing
    job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, nullptr));
    job->start();

    QObject::connect(job, &KJob::result, &app, [&]() {
        if (job->error()) {
            qWarning() << job->errorString();
            app.exit(1);
        } else {
            qDebug() << "Successfully started";
            app.exit(0);
        }
    });

    return app.exec();
}
