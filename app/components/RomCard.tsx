'use client'

import { Play, Download, Info, HardDrive } from 'lucide-react'

interface Rom {
  id: string
  title: string
  platform: string
  region: string
  filePath: string
  fileSize: number
  crc32: string
  sha1: string
}

interface RomCardProps {
  rom: Rom
  onPlay: (rom: Rom) => void
  onDownload: (rom: Rom) => void
  onDetails: (rom: Rom) => void
}

export default function RomCard({ rom, onPlay, onDownload, onDetails }: RomCardProps) {
  const formatFileSize = (bytes: number) => {
    const sizes = ['Bytes', 'KB', 'MB', 'GB']
    if (bytes === 0) return '0 Bytes'
    const i = Math.floor(Math.log(bytes) / Math.log(1024))
    return Math.round(bytes / Math.pow(1024, i) * 100) / 100 + ' ' + sizes[i]
  }

  return (
    <div className="rom-card group">
      {/* ROM Cover/Thumbnail Placeholder */}
      <div className="relative h-48 bg-gradient-to-br from-playstation-blue to-playstation-light-blue rounded-lg mb-4 flex items-center justify-center">
        <div className="text-6xl font-bold text-white/20">PS</div>
        
        {/* Hover overlay with action buttons */}
        <div className="absolute inset-0 bg-black/70 opacity-0 group-hover:opacity-100 transition-opacity rounded-lg flex items-center justify-center space-x-3">
          <button
            onClick={() => onPlay(rom)}
            className="p-3 bg-green-600 hover:bg-green-700 rounded-full transition-colors"
            title="Play ROM"
          >
            <Play size={20} fill="white" />
          </button>
          <button
            onClick={() => onDownload(rom)}
            className="p-3 bg-blue-600 hover:bg-blue-700 rounded-full transition-colors"
            title="Download ROM"
          >
            <Download size={20} />
          </button>
          <button
            onClick={() => onDetails(rom)}
            className="p-3 bg-gray-600 hover:bg-gray-700 rounded-full transition-colors"
            title="ROM Details"
          >
            <Info size={20} />
          </button>
        </div>
      </div>

      {/* ROM Information */}
      <div className="space-y-2">
        <h3 className="font-bold text-lg text-white group-hover:text-playstation-light-blue transition-colors">
          {rom.title}
        </h3>
        
        <div className="flex items-center justify-between text-sm text-gray-400">
          <span className="bg-playstation-blue/20 px-2 py-1 rounded text-playstation-light-blue">
            {rom.platform}
          </span>
          <span>{rom.region}</span>
        </div>

        <div className="flex items-center text-sm text-gray-400">
          <HardDrive size={16} className="mr-1" />
          <span>{formatFileSize(rom.fileSize)}</span>
        </div>

        <div className="text-xs text-gray-500 space-y-1">
          <div>CRC32: {rom.crc32}</div>
          <div>SHA1: {rom.sha1.substring(0, 16)}...</div>
        </div>
      </div>
    </div>
  )
}
