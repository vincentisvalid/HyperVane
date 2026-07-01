'use client'

import { useState } from 'react'
import { Menu, X, Settings, User } from 'lucide-react'

export default function Header() {
  const [isMenuOpen, setIsMenuOpen] = useState(false)

  return (
    <header className="bg-black/50 backdrop-blur-sm border-b border-gray-800">
      <div className="container mx-auto px-4">
        <div className="flex items-center justify-between h-16">
          <div className="flex items-center space-x-4">
            <div className="text-2xl font-bold text-playstation-blue">
              ROMULUS
            </div>
            <nav className="hidden md:flex space-x-6">
              <a href="#" className="text-gray-300 hover:text-white transition-colors">
                Library
              </a>
              <a href="#" className="text-gray-300 hover:text-white transition-colors">
                Downloads
              </a>
              <a href="#" className="text-gray-300 hover:text-white transition-colors">
                Settings
              </a>
            </nav>
          </div>

          <div className="flex items-center space-x-4">
            <button className="p-2 text-gray-300 hover:text-white transition-colors">
              <Settings size={20} />
            </button>
            <button className="p-2 text-gray-300 hover:text-white transition-colors">
              <User size={20} />
            </button>
            <button
              className="md:hidden p-2 text-gray-300 hover:text-white transition-colors"
              onClick={() => setIsMenuOpen(!isMenuOpen)}
            >
              {isMenuOpen ? <X size={20} /> : <Menu size={20} />}
            </button>
          </div>
        </div>

        {/* Mobile menu */}
        {isMenuOpen && (
          <div className="md:hidden py-4 border-t border-gray-800">
            <nav className="flex flex-col space-y-2">
              <a href="#" className="text-gray-300 hover:text-white transition-colors py-2">
                Library
              </a>
              <a href="#" className="text-gray-300 hover:text-white transition-colors py-2">
                Downloads
              </a>
              <a href="#" className="text-gray-300 hover:text-white transition-colors py-2">
                Settings
              </a>
            </nav>
          </div>
        )}
      </div>
    </header>
  )
}
