// Copyright(c) 2017-2019 Alejandro Sirgo Rica & Contributors
//
// This file is part of Flameshot.
//
//     Flameshot is free software: you can redistribute it and/or modify
//     it under the terms of the GNU General Public License as published by
//     the Free Software Foundation, either version 3 of the License, or
//     (at your option) any later version.
//
//     Flameshot is distributed in the hope that it will be useful,
//     but WITHOUT ANY WARRANTY; without even the implied warranty of
//     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//     GNU General Public License for more details.
//
//     You should have received a copy of the GNU General Public License
//     along with Flameshot.  If not, see <http://www.gnu.org/licenses/>.

#include "imguruploader.h"
#include "src/utils/filenamehandler.h"
#include "src/utils/systemnotification.h"
#include "src/widgets/loadspinner.h"
#include "src/widgets/imagelabel.h"
#include "src/widgets/notificationwidget.h"
#include "src/utils/confighandler.h"
#include <sstream>
#include <iostream>
#include <iomanip>
#include <string>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QShortcut>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDrag>
#include <QMimeData>
#include <QBuffer>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>

ImgurUploader::ImgurUploader(const QPixmap &capture, QWidget *parent) :
    QWidget(parent), m_pixmap(capture)
{
    // create tmp png file
    char tmp[] = "/tmp/clip.XXXXXX.png";
    mkstemps(tmp, 4);
    bool success = capture.save(QString(tmp), 0, -1);
    std::string tmp_file(tmp);

    if (success) {
        ConfigHandler config;
        std::stringstream save;
        save << config.getUploadScript().toStdString() << " " << tmp_file;
        system(save.str().c_str());
        SystemNotification().sendMessage(
                QObject::tr("Uploaded done"));
        
    }

    // remove tmp file
    std::stringstream rm;
    rm << " rm -f " << tmp_file;
    system(rm.str().c_str());
}

void ImgurUploader::handleReply(QNetworkReply *reply) {
    m_spinner->deleteLater();
    if (reply->error() == QNetworkReply::NoError) {
        QJsonDocument response = QJsonDocument::fromJson(reply->readAll());
        QJsonObject json = response.object();
        QJsonObject data = json[QStringLiteral("data")].toObject();
        m_imageURL.setUrl(data[QStringLiteral("link")].toString());
        m_deleteImageURL.setUrl(QStringLiteral("https://imgur.com/delete/%1").arg(
                                    data[QStringLiteral("deletehash")].toString()));
        onUploadOk();
    } else {
        m_infoLabel->setText(reply->errorString());
    }
    new QShortcut(Qt::Key_Escape, this, SLOT(close()));
}

void ImgurUploader::startDrag() {
    QMimeData *mimeData = new QMimeData;
    mimeData->setUrls(QList<QUrl> { m_imageURL });
    mimeData->setImageData(m_pixmap);

    QDrag *dragHandler = new QDrag(this);
    dragHandler->setMimeData(mimeData);
    dragHandler->setPixmap(m_pixmap.scaled(256, 256, Qt::KeepAspectRatioByExpanding,
                                           Qt::SmoothTransformation));
    dragHandler->exec();
}

void ImgurUploader::upload() {
    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    m_pixmap.save(&buffer, "PNG");

    QUrlQuery urlQuery;
    urlQuery.addQueryItem(QStringLiteral("title"), QStringLiteral("flameshot_screenshot"));
    QString description = FileNameHandler().parsedPattern();
    urlQuery.addQueryItem(QStringLiteral("description"), description);

    QUrl url(QStringLiteral("https://api.imgur.com/3/image"));
    url.setQuery(urlQuery);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      "application/application/x-www-form-urlencoded");
    request.setRawHeader("Authorization", QStringLiteral("Client-ID %1").arg(IMGUR_CLIENT_ID).toUtf8());

    m_NetworkAM->post(request, byteArray);
}

void ImgurUploader::onUploadOk() {
    m_infoLabel->deleteLater();

    m_notification = new NotificationWidget();
    m_vLayout->addWidget(m_notification);

    ImageLabel *imageLabel = new ImageLabel();
    imageLabel->setScreenshot(m_pixmap);
    imageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    connect(imageLabel, &ImageLabel::dragInitiated, this, &ImgurUploader::startDrag);
    m_vLayout->addWidget(imageLabel);

    m_hLayout = new QHBoxLayout();
    m_vLayout->addLayout(m_hLayout);

    m_copyUrlButton = new QPushButton(tr("Copy URL"));
    m_openUrlButton = new QPushButton(tr("Open URL"));
    m_openDeleteUrlButton = new QPushButton(tr("Delete image"));
    m_toClipboardButton = new QPushButton(tr("Image to Clipboard."));
    m_hLayout->addWidget(m_copyUrlButton);
    m_hLayout->addWidget(m_openUrlButton);
    m_hLayout->addWidget(m_openDeleteUrlButton);
    m_hLayout->addWidget(m_toClipboardButton);

    connect(m_copyUrlButton, &QPushButton::clicked,
            this, &ImgurUploader::copyURL);
    connect(m_openUrlButton, &QPushButton::clicked,
            this, &ImgurUploader::openURL);
    connect(m_openDeleteUrlButton, &QPushButton::clicked,
            this, &ImgurUploader::openDeleteURL);
    connect(m_toClipboardButton, &QPushButton::clicked,
            this, &ImgurUploader::copyImage);
}

void ImgurUploader::openURL() {
    bool successful = QDesktopServices::openUrl(m_imageURL);
    if (!successful) {
        m_notification->showMessage(tr("Unable to open the URL."));
    }
}

void ImgurUploader::copyURL() {
    QApplication::clipboard()->setText(m_imageURL.toString());
    m_notification->showMessage(tr("URL copied to clipboard."));
}

void ImgurUploader::openDeleteURL()
{
    bool successful = QDesktopServices::openUrl(m_deleteImageURL);
    if (!successful) {
        m_notification->showMessage(tr("Unable to open the URL."));
    }
}

void ImgurUploader::copyImage() {
    QApplication::clipboard()->setPixmap(m_pixmap);
    m_notification->showMessage(tr("Screenshot copied to clipboard."));
}
