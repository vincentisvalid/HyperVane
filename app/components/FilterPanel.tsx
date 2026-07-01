'use client'

interface FilterPanelProps {
  selectedPlatform: string
  onPlatformChange: (platform: string) => void
}

export default function FilterPanel({ selectedPlatform, onPlatformChange }: FilterPanelProps) {
  const platforms = [
    'PlayStation',
    'PlayStation 2',
    'PlayStation Portable',
    'PlayStation 3',
    'PlayStation Vita'
  ]

  const regions = [
    'NTSC-U (North America)',
    'PAL (Europe)',
    'NTSC-J (Japan)',
    'NTSC-K (Korea)'
  ]

  return (
    <div className="bg-gray-800/50 rounded-lg p-6 space-y-6">
      <h3 className="text-lg font-bold text-white mb-4">Filters</h3>
      
      {/* Platform Filter */}
      <div>
        <h4 className="text-sm font-semibold text-gray-300 mb-3">Platform</h4>
        <div className="space-y-2">
          {platforms.map((platform) => (
            <label key={platform} className="flex items-center space-x-2 cursor-pointer">
              <input
                type="radio"
                name="platform"
                value={platform}
                checked={selectedPlatform === platform}
                onChange={(e) => onPlatformChange(e.target.value)}
                className="text-playstation-blue focus:ring-playstation-blue"
              />
              <span className="text-sm text-gray-300">{platform}</span>
            </label>
          ))}
        </div>
      </div>

      {/* Region Filter */}
      <div>
        <h4 className="text-sm font-semibold text-gray-300 mb-3">Region</h4>
        <div className="space-y-2">
          {regions.map((region) => (
            <label key={region} className="flex items-center space-x-2 cursor-pointer">
              <input
                type="checkbox"
                className="text-playstation-blue focus:ring-playstation-blue"
              />
              <span className="text-sm text-gray-300">{region}</span>
            </label>
          ))}
        </div>
      </div>

      {/* File Size Filter */}
      <div>
        <h4 className="text-sm font-semibold text-gray-300 mb-3">File Size</h4>
        <div className="space-y-2">
          <label className="flex items-center space-x-2 cursor-pointer">
            <input type="checkbox" className="text-playstation-blue focus:ring-playstation-blue" />
            <span className="text-sm text-gray-300">Under 100 MB</span>
          </label>
          <label className="flex items-center space-x-2 cursor-pointer">
            <input type="checkbox" className="text-playstation-blue focus:ring-playstation-blue" />
            <span className="text-sm text-gray-300">100 MB - 500 MB</span>
          </label>
          <label className="flex items-center space-x-2 cursor-pointer">
            <input type="checkbox" className="text-playstation-blue focus:ring-playstation-blue" />
            <span className="text-sm text-gray-300">500 MB - 1 GB</span>
          </label>
          <label className="flex items-center space-x-2 cursor-pointer">
            <input type="checkbox" className="text-playstation-blue focus:ring-playstation-blue" />
            <span className="text-sm text-gray-300">Over 1 GB</span>
          </label>
        </div>
      </div>

      {/* Clear Filters */}
      <button className="w-full btn-secondary text-sm">
        Clear All Filters
      </button>
    </div>
  )
}
