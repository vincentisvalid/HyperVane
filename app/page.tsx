'use client'

import { useState, useEffect } from 'react'
import { Search, Download, Play, Info, Filter } from 'lucide-react'
import SearchBar from './components/SearchBar'
import RomCard from './components/RomCard'
import FilterPanel from './components/FilterPanel'
import Header from './components/Header'

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

export default function Home() {
  const [searchQuery, setSearchQuery] = useState('')
  const [roms, setRoms] = useState<Rom[]>([])
  const [loading, setLoading] = useState(false)
  const [selectedPlatform, setSelectedPlatform] = useState('PlayStation')
  const [showFilters, setShowFilters] = useState(false)

  // Mock data for demonstration
  const mockRoms: Rom[] = [
    {
      id: '1',
      title: 'Final Fantasy VII',
      platform: 'PlayStation',
      region: 'NTSC-U',
      filePath: '/roms/ff7.bin',
      fileSize: 650000000,
      crc32: 'A1B2C3D4',
      sha1: 'E5F6G7H8I9J0K1L2M3N4O5P6Q7R8S9T0'
    },
    {
      id: '2',
      title: 'Metal Gear Solid',
      platform: 'PlayStation',
      region: 'NTSC-U',
      filePath: '/roms/mgs.bin',
      fileSize: 700000000,
      crc32: 'B2C3D4E5',
      sha1: 'F6G7H8I9J0K1L2M3N4O5P6Q7R8S9T0U1'
    },
    {
      id: '3',
      title: 'Crash Bandicoot',
      platform: 'PlayStation',
      region: 'NTSC-U',
      filePath: '/roms/crash.bin',
      fileSize: 400000000,
      crc32: 'C3D4E5F6',
      sha1: 'G7H8I9J0K1L2M3N4O5P6Q7R8S9T0U1V2'
    }
  ]

  const handleSearch = async (query: string) => {
    setLoading(true)
    setSearchQuery(query)
    
    // Simulate API call
    setTimeout(() => {
      const filtered = mockRoms.filter(rom => 
        rom.title.toLowerCase().includes(query.toLowerCase()) ||
        rom.platform.toLowerCase().includes(query.toLowerCase())
      )
      setRoms(filtered)
      setLoading(false)
    }, 500)
  }

  const handlePlay = (rom: Rom) => {
    console.log('Playing ROM:', rom.title)
    // Implement play functionality
  }

  const handleDownload = (rom: Rom) => {
    console.log('Downloading ROM:', rom.title)
    // Implement download functionality
  }

  const handleDetails = (rom: Rom) => {
    console.log('Showing details for ROM:', rom.title)
    // Implement details modal
  }

  useEffect(() => {
    // Load initial data
    setRoms(mockRoms)
  }, [])

  return (
    <div className="min-h-screen bg-gradient-to-br from-playstation-dark via-gray-900 to-black">
      <Header />
      
      <main className="container mx-auto px-4 py-8">
        <div className="text-center mb-8">
          <h1 className="text-5xl font-bold mb-4 bg-gradient-to-r from-playstation-blue to-playstation-light-blue bg-clip-text text-transparent">
            ROMULUS
          </h1>
          <p className="text-xl text-gray-300 mb-8">
            PlayStation ROM Search & Download Engine
          </p>
          
          <SearchBar onSearch={handleSearch} loading={loading} />
        </div>

        <div className="flex gap-6">
          {/* Filters Sidebar */}
          <div className={`${showFilters ? 'block' : 'hidden'} lg:block w-64 flex-shrink-0`}>
            <FilterPanel 
              selectedPlatform={selectedPlatform}
              onPlatformChange={setSelectedPlatform}
            />
          </div>

          {/* Main Content */}
          <div className="flex-1">
            <div className="flex justify-between items-center mb-6">
              <h2 className="text-2xl font-bold">
                {searchQuery ? `Search Results for "${searchQuery}"` : 'All ROMs'}
                <span className="text-gray-400 text-lg ml-2">({roms.length})</span>
              </h2>
              
              <button
                onClick={() => setShowFilters(!showFilters)}
                className="lg:hidden btn-secondary flex items-center gap-2"
              >
                <Filter size={20} />
                Filters
              </button>
            </div>

            {loading ? (
              <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
                {[...Array(6)].map((_, i) => (
                  <div key={i} className="rom-card animate-pulse">
                    <div className="h-48 bg-gray-700 rounded mb-4"></div>
                    <div className="h-4 bg-gray-700 rounded mb-2"></div>
                    <div className="h-4 bg-gray-700 rounded w-2/3"></div>
                  </div>
                ))}
              </div>
            ) : (
              <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
                {roms.map((rom) => (
                  <RomCard
                    key={rom.id}
                    rom={rom}
                    onPlay={handlePlay}
                    onDownload={handleDownload}
                    onDetails={handleDetails}
                  />
                ))}
              </div>
            )}

            {!loading && roms.length === 0 && searchQuery && (
              <div className="text-center py-12">
                <Search size={64} className="mx-auto text-gray-500 mb-4" />
                <h3 className="text-xl font-semibold mb-2">No ROMs found</h3>
                <p className="text-gray-400">
                  Try adjusting your search terms or filters
                </p>
              </div>
            )}
          </div>
        </div>
      </main>
    </div>
  )
}
