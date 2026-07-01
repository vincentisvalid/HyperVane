'use client'

import { useState } from 'react'
import { Search, Loader2 } from 'lucide-react'

interface SearchBarProps {
  onSearch: (query: string) => void
  loading?: boolean
}

export default function SearchBar({ onSearch, loading = false }: SearchBarProps) {
  const [query, setQuery] = useState('')

  const handleSubmit = (e: React.FormEvent) => {
    e.preventDefault()
    onSearch(query)
  }

  return (
    <form onSubmit={handleSubmit} className="max-w-2xl mx-auto">
      <div className="relative">
        <input
          type="text"
          value={query}
          onChange={(e) => setQuery(e.target.value)}
          placeholder="Search for PlayStation ROMs..."
          className="search-input pr-12"
          disabled={loading}
        />
        <button
          type="submit"
          disabled={loading}
          className="absolute right-3 top-1/2 transform -translate-y-1/2 text-gray-400 hover:text-white transition-colors"
        >
          {loading ? (
            <Loader2 size={20} className="animate-spin" />
          ) : (
            <Search size={20} />
          )}
        </button>
      </div>
    </form>
  )
}
