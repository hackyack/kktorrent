/***************************************************************************
 *   Copyright (C) 2009 by Joris Guisson                                   *
 *   joris.guisson@gmail.com                                               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 ***************************************************************************/



#ifndef KT_WEBVIEW_H
#define KT_WEBVIEW_H

#include <KUrl>
#include <KWebView>
#include <QNetworkReply>


namespace kt
{

    class WebViewClient
    {
    public:
        virtual ~WebViewClient() {}

        /// Get a search url for a search text
        virtual KUrl searchUrl(const QString& search_text) = 0;

        /// Create a new tab
        virtual QWebView* newTab() = 0;

        /// Handle magnet urls
        virtual void magnetUrl(const QUrl& magnet_url) = 0;
    };

    /**
        WebView provides a webkit view which supports for the ktorrent homepage.
     */
    class WebView : public KWebView
    {
        Q_OBJECT
    public:
        WebView(WebViewClient* client, QWidget* parentWidget = 0);
        virtual ~WebView();

        /**
         * Open a url
         * @param url The KUrl
         */
        void openUrl(const KUrl& url);

        /**
         * Show the home page
         */
        void home();

        /**
         * Get a search url for a search text
         * @param search_text The text to search
         * @return A KUrl to load
         */
        KUrl searchUrl(const QString& search_text);

        /**
         * Download a response using KIO
         * @param reply The QNetworkReply to download
         */
        void downloadResponse(QNetworkReply* reply);

        /// Get the html code of the homepage
        QString homePageData();

        /// Get the home page base directory
        QString homePageBaseDir() const {return home_page_base_url;}

        /// Handle magnet url
        void handleMagnetUrl(const QUrl& magnet_url);

    protected:
        void loadHomePage();
        virtual QWebView* createWindow(QWebPage::WebWindowType type);

    public slots:
        /**
         * Download a netwerk request
         * @param req The request
         */
        void downloadRequested(const QNetworkRequest& req);

    private:
        QString home_page_html;
        QString home_page_base_url;
        WebViewClient* client;
        KUrl clicked_url;
        KUrl image_url;
    };

}

#endif // KT_HOMEPAGE_H
