package com.fireball.mini.data

import android.content.Context
import android.content.SharedPreferences
import com.fireball.mini.FireballApp
import com.fireball.mini.core.models.SearchEngine
import com.fireball.mini.core.models.SearchEngineDefaults
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import org.json.JSONArray
import org.json.JSONObject

class SearchEngineRepository(context: Context? = null) {
    private val resolvedContext: Context? = context ?: FireballApp.instance
    private val prefs: SharedPreferences? = try {
        resolvedContext?.getSharedPreferences("fireball_search_engines", Context.MODE_PRIVATE)
    } catch (_: Throwable) {
        null
    }

    private val _availableEngines = MutableStateFlow<List<SearchEngine>>(emptyList())
    val availableEngines: StateFlow<List<SearchEngine>> = _availableEngines.asStateFlow()

    private val _defaultEngine = MutableStateFlow<SearchEngine>(SearchEngineDefaults.DUCKDUCKGO)
    val defaultEngine: StateFlow<SearchEngine> = _defaultEngine.asStateFlow()

    init {
        loadEngines()
    }

    private fun loadEngines() {
        val defaultId = prefs?.getString("default_engine_id", SearchEngineDefaults.DUCKDUCKGO.id)
            ?: SearchEngineDefaults.DUCKDUCKGO.id

        val customEnginesJson = prefs?.getString("custom_engines", "[]") ?: "[]"
        val customList = mutableListOf<SearchEngine>()
        try {
            val arr = JSONArray(customEnginesJson)
            for (i in 0 until arr.length()) {
                val obj = arr.getJSONObject(i)
                customList.add(
                    SearchEngine(
                        id = obj.getString("id"),
                        name = obj.getString("name"),
                        searchUrlTemplate = obj.getString("searchUrlTemplate"),
                        suggestUrlTemplate = if (obj.has("suggestUrlTemplate") && !obj.isNull("suggestUrlTemplate")) obj.getString("suggestUrlTemplate") else null,
                        iconEmoji = if (obj.has("iconEmoji")) obj.getString("iconEmoji") else "🔍",
                        bangPrefix = if (obj.has("bangPrefix") && !obj.isNull("bangPrefix")) obj.getString("bangPrefix") else null,
                        isCustom = true
                    )
                )
            }
        } catch (_: Exception) {}

        val allEngines = SearchEngineDefaults.BUILT_IN_ENGINES + customList
        _availableEngines.value = allEngines
        _defaultEngine.value = allEngines.firstOrNull { it.id == defaultId }
            ?: SearchEngineDefaults.DUCKDUCKGO
    }

    fun setDefaultEngine(engineId: String) {
        val engine = _availableEngines.value.firstOrNull { it.id == engineId } ?: return
        _defaultEngine.value = engine
        prefs?.edit()?.putString("default_engine_id", engineId)?.apply()
    }

    fun addCustomEngine(
        name: String,
        searchUrlTemplate: String,
        suggestUrlTemplate: String? = null,
        bangPrefix: String? = null,
        iconEmoji: String = "🔍"
    ): SearchEngine {
        val id = "custom_${System.currentTimeMillis()}"
        val newEngine = SearchEngine(
            id = id,
            name = name,
            searchUrlTemplate = searchUrlTemplate,
            suggestUrlTemplate = suggestUrlTemplate,
            bangPrefix = if (bangPrefix?.startsWith("!") == true) bangPrefix else if (!bangPrefix.isNullOrBlank()) "!$bangPrefix" else null,
            iconEmoji = iconEmoji,
            isCustom = true
        )

        val updated = _availableEngines.value + newEngine
        _availableEngines.value = updated
        saveCustomEngines()
        return newEngine
    }

    fun deleteCustomEngine(engineId: String) {
        val updated = _availableEngines.value.filterNot { it.id == engineId && it.isCustom }
        _availableEngines.value = updated
        if (_defaultEngine.value.id == engineId) {
            setDefaultEngine(SearchEngineDefaults.DUCKDUCKGO.id)
        }
        saveCustomEngines()
    }

    private fun saveCustomEngines() {
        val customEngines = _availableEngines.value.filter { it.isCustom }
        val arr = JSONArray()
        for (e in customEngines) {
            val obj = JSONObject().apply {
                put("id", e.id)
                put("name", e.name)
                put("searchUrlTemplate", e.searchUrlTemplate)
                put("suggestUrlTemplate", e.suggestUrlTemplate)
                put("iconEmoji", e.iconEmoji)
                put("bangPrefix", e.bangPrefix)
            }
            arr.put(obj)
        }
        prefs?.edit()?.putString("custom_engines", arr.toString())?.apply()
    }
}
