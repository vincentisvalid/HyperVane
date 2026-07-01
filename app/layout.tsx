import './globals.css'
import type { Metadata } from 'next'

export const metadata: Metadata = {
  title: 'ROMULUS - PlayStation ROM Search & Download Engine',
  description: 'Search and download PlayStation ROMs with ROMULUS',
}

export default function RootLayout({
  children,
}: {
  children: React.ReactNode
}) {
  return (
    <html lang="en">
      <body className="bg-playstation-dark text-white font-playstation">
        {children}
      </body>
    </html>
  )
}
